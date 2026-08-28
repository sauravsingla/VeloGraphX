#!/usr/bin/env python3
import argparse
import json
import os
import re
import statistics
import subprocess
from pathlib import Path

THREADS = (1, 2, 4)


def run(argv, *, env=None, stdin=None, allow_failure=False):
    proc = subprocess.run(argv, text=True, input=stdin, capture_output=True, env=env, check=False)
    if proc.returncode != 0 and not allow_failure:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(map(str, argv))}\n{proc.stderr or proc.stdout}")
    return {"argv": [str(x) for x in argv], "returncode": proc.returncode,
            "stdout": proc.stdout, "stderr": proc.stderr}


def generate_graph(outdir: Path):
    n = 4096
    edges = set()
    for u in range(n):
        for delta in (1, 7, 31, 127):
            v = (u + delta) % n
            a, b = sorted((u, v))
            if a != b:
                edges.add((a, b))
    ordered = sorted(edges)
    el = outdir / "hosted-native.el"
    el.write_text("".join(f"{u} {v}\n" for u, v in ordered))
    mtx = outdir / "hosted-native.mtx"
    # LAGraph expects MatrixMarket; symmetric storage describes an undirected graph.
    with mtx.open("w") as f:
        f.write("%%MatrixMarket matrix coordinate pattern symmetric\n")
        f.write(f"{n} {n} {len(ordered)}\n")
        for u, v in ordered:
            f.write(f"{u+1} {v+1}\n")
    return {"vertices": n, "undirected_edges": len(ordered), "edge_list": str(el), "matrix_market": str(mtx)}


def gap_measurements(gap_bfs: Path, dataset: Path):
    rows = []
    trial_re = re.compile(r"Trial Time:\s*([0-9.eE+-]+)")
    for t in THREADS:
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = str(t)
        result = run([gap_bfs, "-sf", dataset, "-n", "5"], env=env)
        combined = result["stdout"] + "\n" + result["stderr"]
        samples = [float(x) for x in trial_re.findall(combined)]
        rows.append({
            "threads": t,
            "returncode": result["returncode"],
            "trial_seconds": samples,
            "median_seconds": statistics.median(samples) if samples else None,
            "verification_passed": "Verification: PASS" in combined,
            "stdout_tail": result["stdout"][-4000:],
            "stderr_tail": result["stderr"][-4000:],
        })
    return rows


def lagraph_measurements(bfs_demo: Path, matrix_market: Path):
    rows = []
    avg_re = re.compile(r"Avg: BFS pushpull parent only, threads\s+\d+:\s+([0-9.eE+-]+) sec")
    matrix_text = matrix_market.read_text()
    for t in THREADS:
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = str(t)
        env["OMP_THREAD_LIMIT"] = str(t)
        result = run([bfs_demo, str(matrix_market)], env=env, stdin=matrix_text)
        combined = result["stdout"] + "\n" + result["stderr"]
        values = [float(x) for x in avg_re.findall(combined)]
        rows.append({
            "threads": t,
            "returncode": result["returncode"],
            "average_seconds": values[-1] if values else None,
            "benchmark_self_check_present": "check:" in combined,
            "stdout_tail": result["stdout"][-4000:],
            "stderr_tail": result["stderr"][-4000:],
        })
    return rows


def perf_attempt(velographx_bench: Path):
    result = run([
        "python3", "tools/hardware_campaign_driver.py", "--threads", "1", "--perf",
        "--perf-events", "cycles,instructions,cache-misses,branches,branch-misses", "--", str(velographx_bench)
    ], allow_failure=True)
    payload = {"status": "unavailable-or-denied", "returncode": result["returncode"],
               "stdout": result["stdout"], "stderr": result["stderr"]}
    if result["returncode"] == 0:
        try:
            payload = {"status": "available", "result": json.loads(result["stdout"])}
        except json.JSONDecodeError:
            payload["status"] = "unexpected-output"
    return payload


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output-dir", type=Path, required=True)
    p.add_argument("--gap-bfs", type=Path, required=True)
    p.add_argument("--lagraph-bfs-demo", type=Path, required=True)
    p.add_argument("--velographx-benchmark", type=Path, required=True)
    args = p.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    graph = generate_graph(args.output_dir)
    report = {
        "schema_version": 1,
        "artifact_type": "velographx-hosted-native-measurements",
        "research_claim": False,
        "publication_grade": False,
        "normalized_cross_engine_claim": False,
        "graph": graph,
        "gap_bfs": gap_measurements(args.gap_bfs, Path(graph["edge_list"])),
        "lagraph_bfs": lagraph_measurements(args.lagraph_bfs_demo, Path(graph["matrix_market"])),
        "velographx_perf": perf_attempt(args.velographx_benchmark),
        "claim_gate": {
            "publication_ready": False,
            "allowed_claim": "Hosted-CI same-runner engineering timing only. GAP and LAGraph upstream harness semantics are recorded separately; no normalized cross-engine speedup claim."
        }
    }
    out = args.output_dir / "hosted-native-measurements.json"
    out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "ok": True,
        "gap_threads": [x["threads"] for x in report["gap_bfs"]],
        "lagraph_threads": [x["threads"] for x in report["lagraph_bfs"]],
        "perf_status": report["velographx_perf"]["status"]
    }))

if __name__ == "__main__":
    main()
