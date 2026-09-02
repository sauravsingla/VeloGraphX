#!/usr/bin/env python3
"""Run a same-machine, same-stream VeloGraphX/NetworKit/RisGraph campaign."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import statistics
import subprocess
from pathlib import Path

RISGRAPH_MEAN = re.compile(r"wall_mean = ([0-9.eE+-]+) us")
RISGRAPH_TOTAL = re.compile(r"wall_times = ([0-9.eE+-]+) us")
RISGRAPH_VISITED = re.compile(r"[0-9]+ visited vertices: ([0-9]+)")


def run(command: list[str], env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=True, env=env)


def parse_risgraph(stdout: str, stderr: str) -> dict[str, object]:
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    if len(lines) < 2:
        raise RuntimeError("RisGraph did not emit incremental and full BFS layers")
    incremental = [int(value) for value in lines[-2].split()]
    full = [int(value) for value in lines[-1].split()]
    mean = RISGRAPH_MEAN.search(stderr)
    total = RISGRAPH_TOTAL.search(stderr)
    visited = RISGRAPH_VISITED.findall(stderr)
    if incremental != full or not mean or not total or not visited:
        raise RuntimeError("RisGraph exactness or timing output validation failed")
    return {
        "wall_mean_us": float(mean.group(1)),
        "wall_total_us": float(total.group(1)),
        "visited_vertices": int(visited[-1]),
        "layer_counts": full,
        "incremental_equals_full_recompute": True,
    }


def mean_stdev(values: list[float]) -> dict[str, float]:
    return {
        "mean_us": statistics.mean(values),
        "stdev_us": statistics.stdev(values) if len(values) > 1 else 0.0,
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--edge-count", type=int, required=True)
    parser.add_argument("--roots", nargs="+", type=int, required=True)
    parser.add_argument("--fractions", nargs="+", type=float, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--base-rate", type=float, default=0.8)
    parser.add_argument("--alignment", type=int, default=1)
    parser.add_argument("--minimum-visited", type=int, default=1000)
    parser.add_argument("--velographx", type=Path, required=True)
    parser.add_argument("--adaptive-policy", type=Path, required=True)
    parser.add_argument("--simple-update-threshold", type=float, default=0.05)
    parser.add_argument("--networkit", type=Path, required=True)
    parser.add_argument("--risgraph", type=Path, required=True)
    parser.add_argument("--risgraph-converter", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if args.repetitions <= 0 or args.alignment <= 0 or not (0 < args.base_rate < 1):
        raise SystemExit("invalid campaign dimensions")

    base_edges = int(args.edge_count * args.base_rate)
    base_edges -= base_edges % args.alignment
    args.output_dir.mkdir(parents=True, exist_ok=True)
    stream_path = args.output_dir / "one-batch-stream.txt"
    binary_path = args.output_dir / "one-batch-stream.bin"
    raw_dir = args.output_dir / "raw"
    raw_dir.mkdir(exist_ok=True)
    env = os.environ.copy()
    env.update({"OMP_NUM_THREADS": "1", "OMP_DYNAMIC": "false", "OMP_PROC_BIND": "true"})
    results: list[dict[str, object]] = []

    for fraction in args.fractions:
        batch_edges = math.ceil(base_edges * fraction)
        batch_edges += (-batch_edges) % args.alignment
        total_edges = base_edges + batch_edges
        if total_edges > args.edge_count:
            raise RuntimeError(f"fraction {fraction} exceeds available stream")
        digest = hashlib.sha256()
        written = 0
        with args.input.open("rb") as source, stream_path.open("wb") as target:
            for line in source:
                if written >= total_edges:
                    break
                target.write(line)
                digest.update(line)
                written += 1
        if written != total_edges:
            raise RuntimeError("input ended before the requested one-batch stream")
        stream_sha256 = digest.hexdigest()
        imported_rate = (base_edges + 0.25) / total_edges
        imported_text = format(imported_rate, ".17g")
        if int(total_edges * float(imported_text)) != base_edges:
            raise RuntimeError("could not encode an exact imported-edge boundary")
        with stream_path.open("rb") as source, binary_path.open("wb") as target:
            subprocess.run([str(args.risgraph_converter)], stdin=source, stdout=target, check=True, env=env)

        for root in args.roots:
            adaptive_process = run(
                [
                    str(args.adaptive_policy),
                    str(stream_path),
                    str(root),
                    imported_text,
                    str(batch_edges),
                    str(args.simple_update_threshold),
                ],
                env,
            )
            adaptive = json.loads(adaptive_process.stdout)
            policies = {row["name"]: row for row in adaptive["policies"]}
            if not adaptive["all_policies_exact"] or adaptive["batches"] != 1:
                raise RuntimeError("adaptive-selector policy or exactness contract failed")
            if set(policies) != {"always_incremental", "always_full", "simple_threshold", "adaptive"}:
                raise RuntimeError("adaptive-selector policy set differs from the frozen contract")
            adaptive_policy = policies["adaptive"]
            adaptive_trace = adaptive_policy["trace"][0]
            adaptive_us = float(adaptive_policy["batch_us"][0])
            oracle_us = float(adaptive["oracle_batch_us"][0])
            adaptive_summary = {
                "selector": adaptive["selector"],
                "all_policies_exact": True,
                "chose_full": adaptive_trace["chose_full"],
                "reason": adaptive_trace["reason"],
                "selector_setup_us": adaptive_policy["selector_setup_us"],
                "decision_us": adaptive_policy["mean_decision_us"],
                "adaptive_us": adaptive_us,
                "oracle_us": oracle_us,
                "oracle_relative_regret": adaptive_us / oracle_us - 1.0,
                "always_incremental_us": policies["always_incremental"]["batch_us"][0],
                "always_full_us": policies["always_full"]["batch_us"][0],
                "feature_cost_included": adaptive["selector_feature_cost_included_in_adaptive_timing"],
            }
            (raw_dir / f"f{fraction:.9g}-r{root}-adaptive.json").write_text(
                json.dumps(adaptive, indent=2, sort_keys=True) + "\n"
            )
            repetitions: list[dict[str, object]] = []
            for repetition in range(1, args.repetitions + 1):
                common = [str(stream_path), str(root), imported_text, str(batch_edges)]
                vx_process = run([str(args.velographx), *common], env)
                nk_process = run([str(args.networkit), *common], env)
                rg_process = run(
                    [str(args.risgraph), str(binary_path), str(root), imported_text, str(batch_edges)], env
                )
                vx = json.loads(vx_process.stdout)
                nk = json.loads(nk_process.stdout)
                rg = parse_risgraph(rg_process.stdout, rg_process.stderr)
                if not vx["correct"] or not vx["legacy_correct"]:
                    raise RuntimeError("VeloGraphX failed independent exactness verification")
                if not nk["all_batches_correct_against_fresh_full_bfs"]:
                    raise RuntimeError("NetworKit failed independent exactness verification")
                scopes = ("root", "initial_edges", "final_edges", "batch_size", "batches", "layer_counts")
                if any(vx[key] != nk[key] for key in scopes):
                    raise RuntimeError("VeloGraphX and NetworKit scopes/results differ")
                if vx["layer_counts"] != rg["layer_counts"]:
                    raise RuntimeError("RisGraph and the independent references differ")
                if int(vx["visited_vertices"]) != int(rg["visited_vertices"]):
                    raise RuntimeError("RisGraph visited count differs from the independent references")
                if int(vx["source_edges"]) != total_edges or int(vx["initial_edges"]) != base_edges:
                    raise RuntimeError("harness imported-edge boundary differs from the requested boundary")
                if int(vx["batches"]) != 1:
                    raise RuntimeError("crossover contract requires exactly one update batch")
                if int(vx["visited_vertices"]) < args.minimum_visited:
                    raise RuntimeError(f"root {root} reaches too few vertices")
                record = {
                    "repetition": repetition,
                    "velographx": vx,
                    "networkit": nk,
                    "risgraph": rg,
                }
                repetitions.append(record)
                stem = f"f{fraction:.9g}-r{root}-n{repetition}"
                (raw_dir / f"{stem}.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
                (raw_dir / f"{stem}.risgraph.stderr").write_text(rg_process.stderr)

            vx_times = [float(row["velographx"]["wall_mean_us"]) for row in repetitions]
            nk_times = [float(row["networkit"]["wall_mean_us"]) for row in repetitions]
            rg_times = [float(row["risgraph"]["wall_mean_us"]) for row in repetitions]
            full_times = [float(row["velographx"]["legacy_full_recompute_mean_us"]) for row in repetitions]
            summary = {
                "fraction": fraction,
                "fraction_percent": fraction * 100.0,
                "batch_edges": batch_edges,
                "update_operations": batch_edges * 2,
                "one_batch_stream_sha256": stream_sha256,
                "one_batch_stream_edges": total_edges,
                "root": root,
                "repetitions": args.repetitions,
                "visited_vertices": repetitions[-1]["velographx"]["visited_vertices"],
                "velographx": mean_stdev(vx_times),
                "networkit": mean_stdev(nk_times),
                "risgraph": mean_stdev(rg_times),
                "velographx_full_recompute": mean_stdev(full_times),
                "velographx_current_policy_over_full_ratio": statistics.mean(vx_times) / statistics.mean(full_times),
                "velographx_over_networkit_ratio": statistics.mean(vx_times) / statistics.mean(nk_times),
                "velographx_over_risgraph_ratio": statistics.mean(vx_times) / statistics.mean(rg_times),
                "adaptive_selector": adaptive_summary,
                "all_exact": True,
            }
            results.append(summary)

    artifact = {
        "schema_version": 1,
        "artifact_type": "same-machine-three-system-dynamic-bfs-campaign",
        "dataset": args.dataset,
        "input_sha256": sha256_file(args.input),
        "source_edge_rows": args.edge_count,
        "base_edges": base_edges,
        "base_rate": args.base_rate,
        "roots": args.roots,
        "fractions": args.fractions,
        "thread_count_per_system": 1,
        "timed_envelope": "graph mutation plus incremental answer maintenance; input/build and fresh correctness BFS excluded",
        "update_semantics": "one batch adds the next source-order window and removes an equal-size oldest source-order window",
        "same_machine_same_stream_same_roots": True,
        "correctness_gate": "VeloGraphX and NetworKit independently checked against fresh BFS; RisGraph incremental checked against its fresh BFS; cross-system layer histograms equal",
        "hosted_ci_engineering_evidence": True,
        "research_claim": False,
        "results": results,
    }
    (args.output_dir / "summary.json").write_text(json.dumps(artifact, indent=2, sort_keys=True) + "\n")
    print(json.dumps(artifact, sort_keys=True))


if __name__ == "__main__":
    main()
