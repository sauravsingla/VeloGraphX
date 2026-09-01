#!/usr/bin/env python3
"""Run a same-input, same-root VeloGraphX vs GAP BFS/SSSP campaign.

The runner validates dataset semantics and provenance before executing any timing.
Canonical status is inherited only from a validated materializer metadata file;
small CI graphs can exercise the same contract but can never become canonical by
naming convention alone.
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

SEMANTICS = {
    "twitter": {"directed": True, "sssp_delta": 2},
    "web": {"directed": True, "sssp_delta": 2},
    "road": {"directed": True, "sssp_delta": 50000},
    "kron": {"directed": False, "sssp_delta": 2},
    "urand": {"directed": False, "sssp_delta": 2},
}


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
        sample = row["kernel_us"] / 1_000_000.0
        if sample < 0:
            raise RuntimeError(f"negative VeloGraphX kernel time: {sample}")
        samples.append(sample)
        digests.append(row["digest"])
        walls.append(wall)
    stable = len(set(digests)) == 1
    if not stable:
        raise RuntimeError(f"VeloGraphX digest changed across repeats: {digests}")
    return {
        "repeat": repeat,
        "source": source,
        "threads": threads,
        "trial_seconds": samples,
        "median_seconds": statistics.median(samples),
        "process_wall_seconds": walls,
        "exact": True,
        "stable_digest": True,
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
        sample = float(values[0])
        if sample < 0:
            raise RuntimeError(f"negative GAP {algorithm} trial time: {sample}")
        samples.append(sample)
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


def parse_int_list(value, name):
    try:
        values = tuple(int(part.strip()) for part in value.split(",") if part.strip())
    except ValueError as exc:
        raise SystemExit(f"{name} must be a comma-separated integer list") from exc
    if not values:
        raise SystemExit(f"{name} must not be empty")
    if len(set(values)) != len(values):
        raise SystemExit(f"{name} must not contain duplicates")
    return values


def validate_metadata(path, args, edge_sha, weighted_sha):
    if path is None:
        return {"provided": False, "canonical": False}
    metadata = json.loads(path.read_text())
    required = {"schema_version", "dataset", "gap_revision", "canonical", "edge_list", "weighted_edge_list", "checksums"}
    missing = sorted(required - set(metadata))
    if missing:
        raise SystemExit(f"dataset metadata missing fields: {missing}")
    if metadata["dataset"] != args.dataset_name:
        raise SystemExit("dataset metadata name does not match --dataset-name")
    if metadata["gap_revision"] != args.gap_revision:
        raise SystemExit("dataset metadata GAP revision does not match --gap-revision")
    if metadata["checksums"].get("edge_list_sha256") != edge_sha:
        raise SystemExit("edge-list checksum does not match dataset metadata")
    if metadata["checksums"].get("weighted_edge_list_sha256") != weighted_sha:
        raise SystemExit("weighted edge-list checksum does not match dataset metadata")
    return {"provided": True, "canonical": metadata["canonical"], "schema_version": metadata["schema_version"]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dataset-name", choices=tuple(SEMANTICS), required=True)
    parser.add_argument("--edge-list", type=Path, required=True)
    parser.add_argument("--weighted-edge-list", type=Path, required=True)
    parser.add_argument("--dataset-metadata", type=Path)
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

    roots = parse_int_list(args.roots, "--roots")
    threads = parse_int_list(args.threads, "--threads")
    if len(roots) < 2:
        raise SystemExit("canonical multiroot campaign requires at least two roots")
    if any(root < 0 for root in roots):
        raise SystemExit("--roots must be non-negative")
    if any(thread <= 0 for thread in threads):
        raise SystemExit("--threads values must be positive")
    if args.repeat < 1:
        raise SystemExit("--repeat must be positive")
    if args.sssp_delta <= 0:
        raise SystemExit("--sssp-delta must be positive")

    expected = SEMANTICS[args.dataset_name]
    if args.directed != expected["directed"]:
        raise SystemExit(f"directedness mismatch for {args.dataset_name}: expected {expected['directed']}")
    if args.sssp_delta != expected["sssp_delta"]:
        raise SystemExit(f"SSSP delta mismatch for {args.dataset_name}: expected {expected['sssp_delta']}")

    required_paths = [args.edge_list, args.weighted_edge_list, args.velographx, args.gap_bfs, args.gap_sssp]
    if args.dataset_metadata is not None:
        required_paths.append(args.dataset_metadata)
    for path in required_paths:
        if not path.exists():
            raise SystemExit(f"missing required path: {path}")

    edge_sha = sha256(args.edge_list)
    weighted_sha = sha256(args.weighted_edge_list)
    provenance = validate_metadata(args.dataset_metadata, args, edge_sha, weighted_sha)

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
        "schema_version": 2,
        "campaign": "gap-canonical-multiroot",
        "dataset": args.dataset_name,
        "gap_revision": args.gap_revision,
        "canonical_dataset_result": bool(provenance["canonical"]),
        "dataset_metadata": provenance,
        "directed": args.directed,
        "roots": roots,
        "threads": threads,
        "repeat": args.repeat,
        "sssp_delta": args.sssp_delta,
        "checksums": {
            "edge_list_sha256": edge_sha,
            "weighted_edge_list_sha256": weighted_sha,
        },
        "input_sizes_bytes": {
            "edge_list": args.edge_list.stat().st_size,
            "weighted_edge_list": args.weighted_edge_list.stat().st_size,
        },
        "openmp": {"OMP_PLACES": "cores", "OMP_PROC_BIND": "spread", "OMP_DYNAMIC": "FALSE"},
        "correctness_gate": {
            "passed": True,
            "velographx_internal_exact": True,
            "velographx_repeat_digest_stable": True,
            "gap_verification_passed_every_run": True,
            "same_roots_and_threads": True,
        },
        "timing_scope": "kernel timers only; dataset materialisation and loading are not mixed into kernel comparison",
        "rows": rows,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "campaign": payload["campaign"],
        "dataset": args.dataset_name,
        "canonical_dataset_result": payload["canonical_dataset_result"],
        "roots": len(roots),
        "thread_counts": len(threads),
        "rows": len(rows),
        "all_correct": True,
        "output": str(args.output),
    }, sort_keys=True))


if __name__ == "__main__":
    main()
