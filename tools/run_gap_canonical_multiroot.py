#!/usr/bin/env python3
"""Run a same-input, same-root VeloGraphX vs GAP BFS/SSSP campaign.

This runner intentionally consumes plain GAP-compatible .el/.wel files. Dataset
materialisation is kept outside timed regions and the caller records the exact
GAP upstream recipe/revision used to create them.
"""

import argparse
import hashlib
import json
import os
import re
import statistics
import subprocess
import time
from pathlib import Path


def run(argv, *, env):
    started = time.perf_counter()
    proc = subprocess.run(argv, text=True, capture_output=True, env=env, check=False)
    wall = time.perf_counter() - started
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {' '.join(map(str, argv))}\n"
            f"stdout:\n{proc.stdout[-4000:]}\nstderr:\n{proc.stderr[-4000:]}"
        )
    return proc.stdout + "\n" + proc.stderr, wall


def sha256(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def env_for_threads(threads):
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(threads)
    env["OMP_THREAD_LIMIT"] = str(threads)
    env["OMP_DYNAMIC"] = "FALSE"
    env["OMP_PLACES"] = "cores"
    env["OMP_PROC_BIND"] = "spread"
    env["VELOGRAPHX_THREADS"] = str(threads)
    return env


def parse_vx(text):
    line = next((line for line in text.splitlines() if line.startswith("{")), None)
    if line is None:
        raise RuntimeError(f"missing VeloGraphX JSON output: {text[-3000:]}")
    row = json.loads(line)
    if row.get("exact") is not True:
        raise RuntimeError(f"VeloGraphX correctness gate failed: {row}")
    return row


def measure_vx(exe, mode, dataset, source, repeat, threads):
    samples, digests, walls = [], [], []
    for _ in range(repeat):
        text, wall = run([str(exe), mode, str(dataset), str(source)], env=env_for_threads(threads))
        row = parse_vx(text)
        samples.append(row["kernel_us"] / 1_000_000.0)
        digests.append(row["digest"])
        walls.append(wall)
    return {
        "repeat": repeat,
        "source": source,
        "threads": threads,
        "trial_seconds": samples,
        "median_seconds": statistics.median(samples),
        "process_wall_seconds": walls,
        "exact": True,
        "stable_digest": len(set(digests)) == 1,
        "digest": digests[0],
        "timing_scope": "kernel-only; input construction excluded",
    }


def measure_gap(exe, algorithm, dataset, source, repeat, threads, delta):
    trial_re = re.compile(r"Trial Time:\s*([0-9.eE+-]+)")
    verification_re = re.compile(r"^Verification:\s*(PASS|FAIL)\s*$", re.MULTILINE)
    samples, walls = [], []
    for _ in range(repeat):
        if algorithm == "bfs":
            argv = [str(exe), "-f", str(dataset), "-r", str(source), "-n", "1", "-v"]
        else:
            argv = [str(exe), "-f", str(dataset), "-r", str(source), "-n", "1", "-v", "-d", str(delta)]
        text, wall = run(argv, env=env_for_threads(threads))
        values = trial_re.findall(text)
        checks = verification_re.findall(text)
        if len(values) != 1 or checks != ["PASS"]:
            raise RuntimeError(f"unexpected GAP {algorithm} verification output: {text[-4000:]}")
        samples.append(float(values[0]))
        walls.append(wall)
    return {
        "repeat": repeat,
        "source": source,
        "threads": threads,
        "trial_seconds": samples,
        "median_seconds": statistics.median(samples),
        "process_wall_seconds": walls,
        "verification_passed": True,
        "timing_scope": "GAP Trial Time kernel-only; input loading and construction excluded from reported kernel timer",
    }


def parse_int_list(value):
    return tuple(int(part.strip()) for part in value.split(",") if part.strip())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dataset-name", choices=("twitter", "web", "road", "kron", "urand"), required=True)
    parser.add_argument("--edge-list", type=Path, required=True)
    parser.add_argument("--weighted-edge-list", type=Path, required=True)
    parser.add_argument("--velographx", type=Path, required=True)
    parser.add_argument("--gap-bfs", type=Path, required=True)
    parser.add_argument("--gap-sssp", type=Path, required=True)
    parser.add_argument("--roots", default="0,1,2,3,7,15,31,63")
    parser.add_argument("--threads", default="1,2,4")
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--directed", action="store_true")
    parser.add_argument("--sssp-delta", type=int, default=2)
    parser.add_argument("--gap-revision", default="v1.5")
    args = parser.parse_args()

    roots = parse_int_list(args.roots)
    threads = parse_int_list(args.threads)
    if len(roots) < 2:
        raise SystemExit("canonical multiroot campaign requires at least two roots")
    if args.repeat < 1:
        raise SystemExit("--repeat must be positive")
    for path in (args.edge_list, args.weighted_edge_list, args.velographx, args.gap_bfs, args.gap_sssp):
        if not path.exists():
            raise SystemExit(f"missing required path: {path}")

    bfs_mode = "bfs-directed" if args.directed else "bfs"
    sssp_mode = "sssp-directed" if args.directed else "sssp"
    rows = []
    for thread_count in threads:
        for source in roots:
            vx_bfs = measure_vx(args.velographx, bfs_mode, args.edge_list, source, args.repeat, thread_count)
            gap_bfs = measure_gap(args.gap_bfs, "bfs", args.edge_list, source, args.repeat, thread_count, args.sssp_delta)
            vx_sssp = measure_vx(args.velographx, sssp_mode, args.weighted_edge_list, source, args.repeat, thread_count)
            gap_sssp = measure_gap(args.gap_sssp, "sssp", args.weighted_edge_list, source, args.repeat, thread_count, args.sssp_delta)
            rows.append({
                "threads": thread_count,
                "source": source,
                "bfs": {
                    "velographx": vx_bfs,
                    "gap": gap_bfs,
                    "vx_over_gap_ratio": vx_bfs["median_seconds"] / gap_bfs["median_seconds"] if gap_bfs["median_seconds"] else None,
                },
                "sssp": {
                    "velographx": vx_sssp,
                    "gap": gap_sssp,
                    "vx_over_gap_ratio": vx_sssp["median_seconds"] / gap_sssp["median_seconds"] if gap_sssp["median_seconds"] else None,
                },
            })

    payload = {
        "schema_version": 1,
        "campaign": "gap-canonical-multiroot",
        "dataset": args.dataset_name,
        "gap_revision": args.gap_revision,
        "directed": args.directed,
        "roots": roots,
        "threads": threads,
        "repeat": args.repeat,
        "sssp_delta": args.sssp_delta,
        "checksums": {
            "edge_list_sha256": sha256(args.edge_list),
            "weighted_edge_list_sha256": sha256(args.weighted_edge_list),
        },
        "openmp": {"OMP_PLACES": "cores", "OMP_PROC_BIND": "spread", "OMP_DYNAMIC": "FALSE"},
        "correctness_gate": "VeloGraphX internal exact oracle and GAP Verification: PASS on every root/repeat",
        "timing_scope": "kernel timers only; dataset materialisation and loading are not mixed into kernel comparison",
        "rows": rows,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "campaign": payload["campaign"],
        "dataset": args.dataset_name,
        "roots": len(roots),
        "thread_counts": len(threads),
        "rows": len(rows),
        "all_correct": True,
        "output": str(args.output),
    }, sort_keys=True))


if __name__ == "__main__":
    main()
