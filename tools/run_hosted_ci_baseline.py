#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

THREADS = [1, 2, 4]
FRAMEWORKS = ["builtin", "networkx", "igraph", "networkit", "rustworkx"]


def run(cmd, *, allow_failure=False):
    proc = subprocess.run(cmd, text=True, capture_output=True, check=False)
    if proc.returncode != 0 and not allow_failure:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(cmd)}\n{proc.stderr or proc.stdout}")
    return {"argv": cmd, "returncode": proc.returncode, "stdout": proc.stdout.strip(), "stderr": proc.stderr.strip()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--dataset", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    cpu_count = os.cpu_count() or 1
    report = {
        "schema_version": 1,
        "kind": "hosted-ci-baseline",
        "research_claim": False,
        "publication_grade": False,
        "cpu_count": cpu_count,
        "thread_scaling": [],
        "competitors": {},
        "ablation": None,
        "perf": None,
    }

    bench = args.build_dir / "velographx_benchmark"
    for threads in [t for t in THREADS if t <= cpu_count]:
        samples = []
        for _ in range(5):
            result = run([sys.executable, "tools/hardware_campaign_driver.py", "--threads", str(threads), "--", str(bench)])
            samples.append(json.loads(result["stdout"]))
        report["thread_scaling"].append({"threads": threads, "samples": samples})

    ablation_path = args.output_dir / "ablation.json"
    run([sys.executable, "tools/ablation_suite.py", "--build-dir", str(args.build_dir), "--output", str(ablation_path)])
    report["ablation"] = json.loads(ablation_path.read_text())

    reference_digest = None
    for framework in FRAMEWORKS:
        out = args.output_dir / f"competitor-{framework}.json"
        result = run([
            sys.executable, "tools/competitor_benchmark.py",
            "--dataset", str(args.dataset),
            "--framework", framework,
            "--algorithm", "bfs",
            "--source", "0",
            "--repeat", "5",
            "--output", str(out),
        ])
        payload = json.loads(out.read_text())
        digest = payload.get("result_digest")
        if framework == "builtin":
            reference_digest = digest
        elif digest != reference_digest:
            raise RuntimeError(f"correctness digest mismatch for {framework}")
        report["competitors"][framework] = payload

    perf_result = run([
        sys.executable, "tools/hardware_campaign_driver.py",
        "--threads", "1",
        "--perf",
        "--perf-events", "cycles,instructions,cache-misses,branches,branch-misses",
        "--", str(bench),
    ], allow_failure=True)
    if perf_result["returncode"] == 0:
        report["perf"] = {"status": "available", "result": json.loads(perf_result["stdout"])}
    else:
        report["perf"] = {"status": "unavailable-or-denied", "returncode": perf_result["returncode"], "stderr": perf_result["stderr"], "stdout": perf_result["stdout"]}

    report["claim_gate"] = {
        "publication_ready": False,
        "allowed_claim": "Hosted-CI engineering baseline only.",
    }
    (args.output_dir / "hosted-ci-baseline.json").write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
    print(json.dumps({"ok": True, "cpu_count": cpu_count, "threads_tested": [x["threads"] for x in report["thread_scaling"]], "perf_status": report["perf"]["status"]}))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
