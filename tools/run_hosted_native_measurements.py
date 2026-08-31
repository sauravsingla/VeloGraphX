#!/usr/bin/env python3
import argparse
import csv
import io
import json
import os
import re
import statistics
import subprocess
from pathlib import Path

THREADS = (1, 2, 4)
REPEAT = 5
SOURCE = 0


def run(argv, *, env=None, stdin=None, allow_failure=False):
    proc = subprocess.run(argv, text=True, input=stdin, capture_output=True, env=env, check=False)
    if proc.returncode != 0 and not allow_failure:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(map(str, argv))}\n{proc.stderr or proc.stdout}")
    return {"argv": [str(x) for x in argv], "returncode": proc.returncode,
            "stdout": proc.stdout, "stderr": proc.stderr}


def threaded_env(threads):
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(threads)
    env["OMP_THREAD_LIMIT"] = str(threads)
    env["VELOGRAPHX_THREADS"] = str(threads)
    return env


def affinity_command(argv, threads):
    if os.name == "posix" and Path("/usr/bin/taskset").exists():
        return ["/usr/bin/taskset", "-c", f"0-{threads - 1}", *argv]
    return argv


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


def velographx_measurements(public_benchmark: Path, dataset: Path):
    rows = []
    for t in THREADS:
        samples = []
        reachable = []
        for _ in range(REPEAT):
            argv = affinity_command([str(public_benchmark), str(dataset), str(SOURCE)], t)
            result = run(argv, env=threaded_env(t))
            reader = csv.DictReader(io.StringIO(result["stdout"]))
            parsed = list(reader)
            if len(parsed) != 1:
                raise RuntimeError(f"unexpected VeloGraphX benchmark CSV for {t} threads: {result['stdout'][-2000:]}")
            row = parsed[0]
            samples.append(float(row["bfs_us"]) / 1_000_000.0)
            reachable.append(int(row["reachable_vertices"]))
        rows.append({
            "threads": t,
            "source": SOURCE,
            "repeat": REPEAT,
            "trial_seconds": samples,
            "median_seconds": statistics.median(samples),
            "reachable_vertices": reachable[0],
            "reachable_consistent": len(set(reachable)) == 1,
            "all_reachable_samples": reachable,
            "timing_scope": "BFS kernel-only timing reported by velographx_public_dataset_benchmark",
        })
    return rows


def gap_measurements(gap_bfs: Path, dataset: Path):
    rows = []
    trial_re = re.compile(r"Trial Time:\s*([0-9.eE+-]+)")
    verification_re = re.compile(r"^Verification:\s*(PASS|FAIL)\s*$", re.MULTILINE)
    for t in THREADS:
        result = run(affinity_command([str(gap_bfs), "-sf", str(dataset), "-r", str(SOURCE), "-n", str(REPEAT), "-v"], t), env=threaded_env(t))
        combined = result["stdout"] + "\n" + result["stderr"]
        samples = [float(x) for x in trial_re.findall(combined)]
        verification_results = verification_re.findall(combined)
        pass_count = sum(x == "PASS" for x in verification_results)
        fail_count = sum(x == "FAIL" for x in verification_results)
        verification_passed = len(samples) == REPEAT and pass_count == REPEAT and fail_count == 0
        rows.append({
            "threads": t,
            "source": SOURCE,
            "repeat": REPEAT,
            "returncode": result["returncode"],
            "trial_seconds": samples,
            "median_seconds": statistics.median(samples) if samples else None,
            "verification_passed": verification_passed,
            "verification_pass_count": pass_count,
            "verification_fail_count": fail_count,
            "verification_result_count": len(verification_results),
            "stdout_tail": result["stdout"][-4000:],
            "stderr_tail": result["stderr"][-4000:],
        })
    return rows


def lagraph_measurements(bfs_demo: Path, matrix_market: Path):
    rows = []
    avg_re = re.compile(r"Avg: BFS pushpull parent only, threads\s+\d+:\s+([0-9.eE+-]+) sec")
    matrix_text = matrix_market.read_text()
    for t in THREADS:
        result = run(affinity_command([str(bfs_demo), str(matrix_market)], t), env=threaded_env(t), stdin=matrix_text)
        combined = result["stdout"] + "\n" + result["stderr"]
        values = [float(x) for x in avg_re.findall(combined)]
        check_match = re.search(r"\bn:\s*[0-9.eE+-]+\s+check:\s*([0-9.eE+-]+)\s+sec", combined)
        rows.append({
            "threads": t,
            "returncode": result["returncode"],
            "average_seconds": values[-1] if values else None,
            "benchmark_self_check_present": check_match is not None,
            "self_check_seconds": float(check_match.group(1)) if check_match else None,
            "source_semantics": "LAGraph v1.2.2 bfs_demo upstream source set; correctness self-check enabled at build time",
            "same_source_as_velographx_gap": False,
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
    p.add_argument("--velographx-public-benchmark", type=Path, required=True)
    args = p.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    graph = generate_graph(args.output_dir)
    vx = velographx_measurements(args.velographx_public_benchmark, Path(graph["edge_list"]))
    gap = gap_measurements(args.gap_bfs, Path(graph["edge_list"]))
    lagraph = lagraph_measurements(args.lagraph_bfs_demo, Path(graph["matrix_market"]))

    vx_exact = all(x["reachable_consistent"] and x["reachable_vertices"] == graph["vertices"] for x in vx)
    gap_exact = all(x["verification_passed"] for x in gap)
    lagraph_exact = all(x["benchmark_self_check_present"] for x in lagraph)
    correctness_gate = vx_exact and gap_exact and lagraph_exact

    paired_vx_gap = []
    for vx_row, gap_row in zip(vx, gap):
        assert vx_row["threads"] == gap_row["threads"]
        paired_vx_gap.append({
            "threads": vx_row["threads"],
            "source": SOURCE,
            "velographx_median_seconds": vx_row["median_seconds"],
            "gap_median_seconds": gap_row["median_seconds"],
            "vx_over_gap_ratio": (vx_row["median_seconds"] / gap_row["median_seconds"])
                if gap_row["median_seconds"] else None,
            "same_generated_graph": True,
            "same_source": True,
            "both_correctness_gated": vx_exact and gap_exact,
        })

    report = {
        "schema_version": 3,
        "artifact_type": "velographx-hosted-native-measurements",
        "research_claim": False,
        "publication_grade": False,
        "normalized_cross_engine_claim": True,
        "normalized_scope": "VeloGraphX vs GAP BFS only: same generated graph, source 0, thread affinity, five repetitions, and correctness gates. LAGraph/GraphBLAS is same-runner/same-graph build and timing evidence but excluded from normalized speedup because upstream bfs_demo selects its own source set.",
        "graph": graph,
        "velographx_bfs": vx,
        "gap_bfs": gap,
        "lagraph_graphblas_bfs": lagraph,
        "paired_velographx_gap": paired_vx_gap,
        "correctness_gate": {
            "passed": correctness_gate,
            "velographx_reachable_all_vertices_all_trials": vx_exact,
            "gap_all_trials_verified": gap_exact,
            "lagraph_self_check_present_all_threads": lagraph_exact,
            "same_source_velographx_gap_proven": True,
            "same_source_lagraph_proven": False,
        },
        "velographx_perf": perf_attempt(args.velographx_benchmark),
        "claim_gate": {
            "publication_ready": False,
            "allowed_claim": "Hosted-CI engineering evidence: normalized same-source VeloGraphX-vs-GAP BFS plus same-runner LAGraph/GraphBLAS timing/self-check. No publication-grade superiority or scalability claim."
        }
    }
    out = args.output_dir / "hosted-native-measurements.json"
    out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if not correctness_gate:
        raise RuntimeError("native competitor correctness gate failed; see hosted-native-measurements.json")
    print(json.dumps({
        "ok": True,
        "correctness_gate": correctness_gate,
        "velographx_threads": [x["threads"] for x in vx],
        "gap_threads": [x["threads"] for x in gap],
        "lagraph_threads": [x["threads"] for x in lagraph],
        "perf_status": report["velographx_perf"]["status"]
    }))

if __name__ == "__main__":
    main()
