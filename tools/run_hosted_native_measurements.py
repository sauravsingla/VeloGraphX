#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import re
import statistics
import subprocess
import time
from pathlib import Path

THREADS = (1, 2, 4)
REPEAT = 5
SOURCE = 0
DYNAMIC_FRACTIONS = (0.001, 0.01, 0.05)


def run(argv, *, env=None, allow_failure=False):
    start = time.perf_counter()
    proc = subprocess.run(argv, text=True, capture_output=True, env=env, check=False)
    wall = time.perf_counter() - start
    if proc.returncode != 0 and not allow_failure:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(map(str, argv))}\n"
            f"stdout:\n{proc.stdout[-4000:]}\nstderr:\n{proc.stderr[-4000:]}"
        )
    return {
        "argv": [str(x) for x in argv],
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "wall_seconds": wall,
    }


def threaded_env(threads):
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(threads)
    env["OMP_THREAD_LIMIT"] = str(threads)
    env["OMP_DYNAMIC"] = "FALSE"
    env["OMP_PLACES"] = "cores"
    env["OMP_PROC_BIND"] = "spread"
    env["VELOGRAPHX_THREADS"] = str(threads)
    return env


def sha256(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_sources(path: Path, source: int):
    path.write_text(
        "%%MatrixMarket matrix coordinate integer general\n"
        "% one 1-based source vertex for LAGraph benchmark demos\n"
        f"1 1 1\n1 1 {source + 1}\n"
    )


def write_graph_files(outdir: Path, edges, weighted, prefix: str):
    el = outdir / f"{prefix}.el"
    wel = outdir / f"{prefix}.wel"
    mtx = outdir / f"{prefix}.mtx"
    wmtx = outdir / f"{prefix}.weighted.mtx"
    el.write_text("".join(f"{u} {v}\n" for u, v in edges))
    wel.write_text("".join(f"{u} {v} {weighted[(u, v)]}\n" for u, v in edges))
    n = max(max(u, v) for u, v in edges) + 1
    with mtx.open("w") as f:
        f.write("%%MatrixMarket matrix coordinate pattern symmetric\n")
        f.write(f"{n} {n} {len(edges)}\n")
        for u, v in edges:
            f.write(f"{u + 1} {v + 1}\n")
    with wmtx.open("w") as f:
        f.write("%%MatrixMarket matrix coordinate integer symmetric\n")
        f.write(f"{n} {n} {len(edges)}\n")
        for u, v in edges:
            f.write(f"{u + 1} {v + 1} {weighted[(u, v)]}\n")
    return {"el": el, "wel": wel, "mtx": mtx, "wmtx": wmtx, "vertices": n}


def generate_graph(outdir: Path):
    n = 32768
    base = set()
    candidates = set()
    for u in range(n):
        for delta in (1, 7, 31, 127):
            v = (u + delta) % n
            a, b = sorted((u, v))
            if a != b:
                base.add((a, b))
        for delta in (3, 11, 47, 191):
            v = (u + delta) % n
            a, b = sorted((u, v))
            if a != b and (a, b) not in base:
                candidates.add((a, b))
    base = sorted(base)
    candidates = sorted(candidates - set(base))
    weights = {}
    for u, v in base + candidates:
        weights[(u, v)] = 1 + ((u * 17 + v * 31) % 16)
    files = write_graph_files(outdir, base, weights, "hosted-native-base")
    source_mtx = outdir / "source-0.mtx"
    write_sources(source_mtx, SOURCE)
    files.update({
        "source_mtx": source_mtx,
        "base_edges": len(base),
        "candidate_edges": len(candidates),
        "weights": weights,
        "base": base,
        "candidates": candidates,
    })
    return files


def parse_vx(result):
    line = next((x for x in result["stdout"].splitlines() if x.startswith("{")), None)
    if not line:
        raise RuntimeError(f"missing VeloGraphX JSON output: {result['stdout'][-2000:]}")
    payload = json.loads(line)
    payload["process_wall_seconds"] = result["wall_seconds"]
    return payload


def vx_static(exe: Path, algorithm: str, dataset: Path, threads: int):
    rows = []
    for _ in range(REPEAT):
        rows.append(parse_vx(run([str(exe), algorithm, str(dataset), str(SOURCE)], env=threaded_env(threads))))
    return summarize_vx(rows, threads)


def vx_dynamic(exe: Path, algorithm: str, base: Path, updates: Path):
    rows = []
    mode = "dynamic-bfs" if algorithm == "bfs" else "dynamic-sssp"
    for _ in range(REPEAT):
        rows.append(parse_vx(run([str(exe), mode, str(base), str(updates), str(SOURCE)], env=threaded_env(1))))
    exact = all(x["exact"] for x in rows)
    return {
        "threads": 1,
        "repeat": REPEAT,
        "source": SOURCE,
        "trial_seconds": [x["kernel_us"] / 1_000_000.0 for x in rows],
        "median_seconds": statistics.median(x["kernel_us"] / 1_000_000.0 for x in rows),
        "full_recompute_trial_seconds": [x["full_recompute_us"] / 1_000_000.0 for x in rows],
        "full_recompute_median_seconds": statistics.median(x["full_recompute_us"] / 1_000_000.0 for x in rows),
        "update_count": rows[0]["update_count"],
        "exact": exact,
        "digests": [x["digest"] for x in rows],
        "timing_scope": "apply update batch plus exact incremental repair; full_recompute excludes input loading",
    }


def summarize_vx(rows, threads):
    exact = all(x["exact"] for x in rows)
    return {
        "threads": threads,
        "repeat": REPEAT,
        "source": SOURCE,
        "trial_seconds": [x["kernel_us"] / 1_000_000.0 for x in rows],
        "median_seconds": statistics.median(x["kernel_us"] / 1_000_000.0 for x in rows),
        "process_wall_seconds": [x["process_wall_seconds"] for x in rows],
        "exact": exact,
        "digests": [x["digest"] for x in rows],
        "timing_scope": "kernel-only; graph representation prepared before timing",
    }


def gap_measure(exe: Path, algorithm: str, dataset: Path, threads: int):
    trials, walls, verifications = [], [], []
    trial_re = re.compile(r"Trial Time:\s*([0-9.eE+-]+)")
    verification_re = re.compile(r"^Verification:\s*(PASS|FAIL)\s*$", re.MULTILINE)
    for _ in range(REPEAT):
        if algorithm == "bfs":
            argv = [str(exe), "-s", "-f", str(dataset), "-r", str(SOURCE), "-n", "1", "-v"]
        else:
            argv = [str(exe), "-f", str(dataset), "-r", str(SOURCE), "-n", "1", "-v", "-d", "2"]
        result = run(argv, env=threaded_env(threads))
        combined = result["stdout"] + "\n" + result["stderr"]
        values = trial_re.findall(combined)
        checks = verification_re.findall(combined)
        if len(values) != 1:
            raise RuntimeError(f"unexpected GAP {algorithm} output: {combined[-3000:]}")
        trials.append(float(values[0]))
        walls.append(result["wall_seconds"])
        verifications.append(checks == ["PASS"])
    return {
        "threads": threads,
        "repeat": REPEAT,
        "source": SOURCE,
        "trial_seconds": trials,
        "median_seconds": statistics.median(trials),
        "process_wall_seconds": walls,
        "process_wall_median_seconds": statistics.median(walls),
        "verification_passed": all(verifications),
        "timing_scope": "GAP Trial Time kernel-only; process wall includes input loading and graph construction",
    }


def lagraph_measure(exe: Path, algorithm: str, matrix: Path, sources: Path, threads: int):
    trials, walls, checks, source_seen = [], [], [], []
    if algorithm == "bfs":
        trial_re = re.compile(r"parent only\s+pushpull trial:\s*0\s+threads:\s*\d+\s+src:\s*(-?\d+)\s+([0-9.eE+-]+) sec")
        check_re = re.compile(r"\bn:\s*[0-9.eE+-]+\s+check:\s*([0-9.eE+-]+)\s+sec")
    else:
        trial_re = re.compile(r"sssp15:\s+threads:\s*\d+\s+trial:\s*0\s+source\s+(-?\d+)\s+time:\s*([0-9.eE+-]+) sec")
        check_re = re.compile(r"total check time:\s*([0-9.eE+-]+)\s+sec")
    for _ in range(REPEAT):
        argv = [str(exe), str(matrix), str(sources)]
        if algorithm == "sssp":
            argv.append("2")
        result = run(argv, env=threaded_env(threads))
        combined = result["stdout"] + "\n" + result["stderr"]
        match = trial_re.search(combined)
        if not match:
            raise RuntimeError(f"unexpected LAGraph {algorithm} output: {combined[-4000:]}")
        source_seen.append(int(match.group(1)))
        trials.append(float(match.group(2)))
        walls.append(result["wall_seconds"])
        checks.append(check_re.search(combined) is not None)
    return {
        "threads": threads,
        "repeat": REPEAT,
        "source": SOURCE,
        "source_seen": source_seen,
        "same_source_proven": all(x == SOURCE for x in source_seen),
        "trial_seconds": trials,
        "median_seconds": statistics.median(trials),
        "process_wall_seconds": walls,
        "process_wall_median_seconds": statistics.median(walls),
        "self_check_passed": all(checks),
        "timing_scope": "LAGraph kernel timer; process wall includes input loading and one-time cached graph properties/transpose work",
    }


def pair_static(vx, gap, lagraph):
    return {
        "threads": vx["threads"],
        "source": SOURCE,
        "velographx_median_seconds": vx["median_seconds"],
        "gap_median_seconds": gap["median_seconds"],
        "lagraph_median_seconds": lagraph["median_seconds"],
        "vx_over_gap_ratio": vx["median_seconds"] / gap["median_seconds"] if gap["median_seconds"] else None,
        "vx_over_lagraph_ratio": vx["median_seconds"] / lagraph["median_seconds"] if lagraph["median_seconds"] else None,
        "same_source": lagraph["same_source_proven"],
        "all_correct": vx["exact"] and gap["verification_passed"] and lagraph["self_check_passed"],
    }


def dynamic_campaign(graph, outdir, vx_exe, gap_bfs, gap_sssp, lagraph_bfs, lagraph_sssp):
    results = []
    base = graph["base"]
    candidates = graph["candidates"]
    weights = graph["weights"]
    for fraction in DYNAMIC_FRACTIONS:
        count = max(1, int(round(len(base) * fraction)))
        updates = candidates[:count]
        update_el = outdir / f"updates-{fraction:.3f}.el"
        update_wel = outdir / f"updates-{fraction:.3f}.wel"
        update_el.write_text("".join(f"{u} {v}\n" for u, v in updates))
        update_wel.write_text("".join(f"{u} {v} {weights[(u, v)]}\n" for u, v in updates))
        snapshot = write_graph_files(outdir, sorted(base + updates), weights, f"snapshot-{fraction:.3f}")

        vx_bfs = vx_dynamic(vx_exe, "bfs", graph["el"], update_el)
        vx_sssp = vx_dynamic(vx_exe, "sssp", graph["wel"], update_wel)
        gap_b = gap_measure(gap_bfs, "bfs", snapshot["el"], 1)
        gap_s = gap_measure(gap_sssp, "sssp", snapshot["wmtx"], 1)
        lg_b = lagraph_measure(lagraph_bfs, "bfs", snapshot["mtx"], graph["source_mtx"], 1)
        lg_s = lagraph_measure(lagraph_sssp, "sssp", snapshot["wmtx"], graph["source_mtx"], 1)

        results.append({
            "fraction": fraction,
            "update_count": count,
            "snapshot_checksums": {k: sha256(v) for k, v in snapshot.items() if isinstance(v, Path)},
            "bfs": {
                "velographx_incremental": vx_bfs,
                "gap_full_recompute": gap_b,
                "lagraph_full_recompute": lg_b,
                "incremental_over_vx_full_ratio": vx_bfs["median_seconds"] / vx_bfs["full_recompute_median_seconds"],
                "incremental_over_gap_kernel_ratio": vx_bfs["median_seconds"] / gap_b["median_seconds"],
                "incremental_over_lagraph_kernel_ratio": vx_bfs["median_seconds"] / lg_b["median_seconds"],
                "incremental_over_gap_end_to_end_ratio": vx_bfs["median_seconds"] / gap_b["process_wall_median_seconds"],
                "incremental_over_lagraph_end_to_end_ratio": vx_bfs["median_seconds"] / lg_b["process_wall_median_seconds"],
            },
            "sssp": {
                "velographx_incremental": vx_sssp,
                "gap_full_recompute": gap_s,
                "lagraph_full_recompute": lg_s,
                "incremental_over_vx_full_ratio": vx_sssp["median_seconds"] / vx_sssp["full_recompute_median_seconds"],
                "incremental_over_gap_kernel_ratio": vx_sssp["median_seconds"] / gap_s["median_seconds"],
                "incremental_over_lagraph_kernel_ratio": vx_sssp["median_seconds"] / lg_s["median_seconds"],
                "incremental_over_gap_end_to_end_ratio": vx_sssp["median_seconds"] / gap_s["process_wall_median_seconds"],
                "incremental_over_lagraph_end_to_end_ratio": vx_sssp["median_seconds"] / lg_s["process_wall_median_seconds"],
            },
        })
    return results


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output-dir", type=Path, required=True)
    p.add_argument("--gap-bfs", type=Path, required=True)
    p.add_argument("--gap-sssp", type=Path, required=True)
    p.add_argument("--lagraph-bfs-demo", type=Path, required=True)
    p.add_argument("--lagraph-sssp-demo", type=Path, required=True)
    p.add_argument("--velographx-native-benchmark", type=Path, required=True)
    args = p.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    graph = generate_graph(args.output_dir)
    static = {"bfs": [], "sssp": []}
    for threads in THREADS:
        vx_b = vx_static(args.velographx_native_benchmark, "bfs", graph["el"], threads)
        gap_b = gap_measure(args.gap_bfs, "bfs", graph["el"], threads)
        lg_b = lagraph_measure(args.lagraph_bfs_demo, "bfs", graph["mtx"], graph["source_mtx"], threads)
        static["bfs"].append({"velographx": vx_b, "gap": gap_b, "lagraph": lg_b, "paired": pair_static(vx_b, gap_b, lg_b)})

        vx_s = vx_static(args.velographx_native_benchmark, "sssp", graph["wel"], threads)
        gap_s = gap_measure(args.gap_sssp, "sssp", graph["wmtx"], threads)
        lg_s = lagraph_measure(args.lagraph_sssp_demo, "sssp", graph["wmtx"], graph["source_mtx"], threads)
        static["sssp"].append({"velographx": vx_s, "gap": gap_s, "lagraph": lg_s, "paired": pair_static(vx_s, gap_s, lg_s)})

    dynamic = dynamic_campaign(
        graph, args.output_dir, args.velographx_native_benchmark,
        args.gap_bfs, args.gap_sssp, args.lagraph_bfs_demo, args.lagraph_sssp_demo,
    )

    all_static_exact = all(
        row["paired"]["all_correct"] and row["paired"]["same_source"]
        for algorithm in static.values() for row in algorithm
    )
    all_dynamic_exact = all(
        regime[alg]["velographx_incremental"]["exact"]
        and regime[alg]["gap_full_recompute"]["verification_passed"]
        and regime[alg]["lagraph_full_recompute"]["self_check_passed"]
        and regime[alg]["lagraph_full_recompute"]["same_source_proven"]
        for regime in dynamic for alg in ("bfs", "sssp")
    )

    report = {
        "schema_version": 4,
        "artifact_type": "velographx-davis-native-baseline-campaign",
        "research_claim": False,
        "publication_grade": False,
        "methodology_source": "Timothy A. Davis guidance, 2026-08-31; LAGraph v1.3.x do_gap_all conventions",
        "openmp": {
            "OMP_PLACES": "cores",
            "OMP_PROC_BIND": "spread",
            "OMP_DYNAMIC": "FALSE",
            "thread_counts": list(THREADS),
        },
        "timing_contract": {
            "primary_static": "kernel-only; file loading and one-time representation construction excluded",
            "dynamic_incremental": "VeloGraphX update application plus exact repair",
            "dynamic_external_kernel": "external optimized full-recompute kernel on post-update snapshot",
            "dynamic_external_end_to_end": "separately retained process wall time, including fresh input load and representation construction; never mixed with kernel-only numbers",
        },
        "graph": {
            "vertices": graph["vertices"],
            "base_edges": graph["base_edges"],
            "candidate_edges": graph["candidate_edges"],
            "source": SOURCE,
            "checksums": {
                "edge_list": sha256(graph["el"]),
                "weighted_edge_list": sha256(graph["wel"]),
                "matrix_market": sha256(graph["mtx"]),
                "weighted_matrix_market": sha256(graph["wmtx"]),
                "source_matrix": sha256(graph["source_mtx"]),
            },
        },
        "static": static,
        "dynamic_crossover": dynamic,
        "correctness_gate": {
            "passed": all_static_exact and all_dynamic_exact,
            "static_all_engines_exact": all_static_exact,
            "dynamic_all_engines_exact": all_dynamic_exact,
            "same_source_all_engines": all_static_exact and all_dynamic_exact,
        },
        "claim_gate": {
            "publication_ready": False,
            "allowed_claim": "Hosted-CI correctness-gated engineering evidence only. Results are workload-specific; controlled hardware remains required for publication-grade scalability claims.",
        },
    }
    out = args.output_dir / "davis-native-baseline-campaign.json"
    out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if not report["correctness_gate"]["passed"]:
        raise RuntimeError("Davis native baseline correctness gate failed")
    print(json.dumps({
        "ok": True,
        "schema_version": report["schema_version"],
        "static_bfs_threads": [x["paired"]["threads"] for x in static["bfs"]],
        "static_sssp_threads": [x["paired"]["threads"] for x in static["sssp"]],
        "dynamic_fractions": [x["fraction"] for x in dynamic],
    }))


if __name__ == "__main__":
    main()
