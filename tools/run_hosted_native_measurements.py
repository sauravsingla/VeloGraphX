#!/usr/bin/env python3
import argparse
import json
import os
import re
import statistics
import subprocess
from pathlib import Path

THREADS = (1, 2, 4)
SOURCE = 0


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
    with mtx.open("w") as f:
        f.write("%%MatrixMarket matrix coordinate pattern symmetric\n")
        f.write(f"{n} {n} {len(ordered)}\n")
        for u, v in ordered:
            f.write(f"{u+1} {v+1}\n")
    return {"vertices": n, "undirected_edges": len(ordered), "edge_list": str(el),
            "matrix_market": str(mtx), "source": SOURCE}


def gap_measurements(gap_bfs: Path, dataset: Path):
    rows = []
    trial_re = re.compile(r"Trial Time:\s*([0-9.eE+-]+)")
    for t in THREADS:
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = str(t)
        result = run([gap_bfs, "-sf", dataset, "-r", str(SOURCE), "-n", "5", "-v"], env=env)
        combined = result["stdout"] + "\n" + result["stderr"]
        samples = [float(x) for x in trial_re.findall(combined)]
        verification_count = combined.count("Verification: PASS")
        rows.append({
            "threads": t,
            "source": SOURCE,
            "returncode": result["returncode"],
            "trial_seconds": samples,
            "median_seconds": statistics.median(samples) if samples else None,
            "verification_passed": verification_count == 5,
            "verification_pass_count": verification_count,
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
        check_match = re.search(r"\bn:\s*[0-9.eE+-]+\s+check:\s*([0-9.eE+-]+)\s+sec", combined)
        rows.append({
            "threads": t,
            "returncode": result["returncode"],
            "average_seconds": values[-1] if values else None,
            "benchmark_self_check_present": check_match is not None,
            "self_check_seconds": float(check_match.group(1)) if check_match else None,
            "source_semantics": "LAGraph upstream demo source set; correctness self-check enabled at build time",
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
    gap = gap_measurements(args.gap_bfs, Path(graph["edge_list"]))
    lagraph = lagraph_measurements(args.lagraph_bfs_demo, Path(graph["matrix_market"]))
    correctness_gate = all(x["verification_passed"] for x in gap) and all(
        x["benchmark_self_check_present"] for x in lagraph)
    report = {
        "schema_version": 2,
        "artifact_type": "velographx-hosted-native-measurements",
        "research_claim": False,
        "publication_grade": False,
        "normalized_cross_engine_claim": False,
        "graph": graph,
        "gap_bfs": gap,
        "lagraph_bfs": lagraph,
        "correctness_gate": {
            "passed": correctness_gate,
            "gap_all_trials_verified": all(x["verification_passed"] for x in gap),
            "lagraph_self_check_present_all_threads": all(x["benchmark_self_check_present"] for x in lagraph),
            "same_source_cross_engine_proven": False
        },
        "velographx_perf": perf_attempt(args.velographx_benchmark),
        "claim_gate": {
            "publication_ready": False,
            "allowed_claim": "Hosted-CI same-runner engineering timing with upstream correctness checks. Cross-engine source/output semantics are not yet fully normalized; no normalized speedup or publication-grade claim."
        }
    }
    out = args.output_dir / "hosted-native-measurements.json"
    out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if not correctness_gate:
        raise RuntimeError("native competitor correctness gate failed")
    print(json.dumps({
        "ok": True,
        "correctness_gate": correctness_gate,
        "gap_threads": [x["threads"] for x in gap],
        "lagraph_threads": [x["threads"] for x in lagraph],
        "perf_status": report["velographx_perf"]["status"]
    }))

if __name__ == "__main__":
    main()
