#!/usr/bin/env python3
import argparse
import csv
import json
import math
import pathlib
import statistics
from collections import defaultdict


def p95(values):
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot compute p95 of empty input")
    return ordered[max(0, math.ceil(0.95 * len(ordered)) - 1)]


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize VeloGraphX public update-fraction benchmark output.")
    parser.add_argument("--input", type=pathlib.Path, required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--family", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    rows = list(csv.DictReader(args.input.open(newline="", encoding="utf-8")))
    if not rows:
        raise ValueError("benchmark CSV contains no rows")
    if any(row["correct"] != "1" for row in rows):
        raise ValueError("incremental result disagrees with full recomputation")
    if any(int(row["changed_edges"]) != int(row["requested_edges"]) for row in rows):
        raise ValueError("requested and changed edge counts differ")

    grouped = defaultdict(list)
    for row in rows:
        grouped[float(row["update_fraction"])].append(row)

    fractions = {}
    crossover = None
    for fraction in sorted(grouped):
        group = grouped[fraction]
        incremental = [int(row["incremental_ns"]) for row in group]
        full = [int(row["full_recompute_ns"]) for row in group]
        speedups = [float(row["speedup"]) for row in group]
        changed = [int(row["changed_edges"]) for row in group]
        requested = [int(row["requested_edges"]) for row in group]
        median_speedup = statistics.median(speedups)
        key = format(fraction, ".12g")
        fractions[key] = {
            "samples": len(group),
            "requested_edges": {
                "min": min(requested),
                "median": statistics.median(requested),
                "max": max(requested),
            },
            "actual_changed_edges": {
                "min": min(changed),
                "median": statistics.median(changed),
                "max": max(changed),
            },
            "median_incremental_ns": statistics.median(incremental),
            "p95_incremental_ns": p95(incremental),
            "median_full_recompute_ns": statistics.median(full),
            "p95_full_recompute_ns": p95(full),
            "median_speedup": median_speedup,
            "p95_speedup": p95(speedups),
            "correctness_rate": 1.0,
        }
        if crossover is None and median_speedup <= 1.0:
            crossover = fraction

    vertex_counts = {int(row["vertices"]) for row in rows}
    base_edges = {int(row["base_edges"]) for row in rows}
    if len(vertex_counts) != 1 or len(base_edges) != 1:
        raise ValueError("graph size changed within campaign")
    repeats = {len(group) for group in grouped.values()}
    if len(repeats) != 1 or next(iter(repeats)) < 2:
        raise ValueError("every fraction must have repeated measurements")

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-multi-dataset-update-crossover-summary",
        "dataset": args.dataset,
        "family": args.family,
        "research_claim": False,
        "vertices": next(iter(vertex_counts)),
        "base_edges": next(iter(base_edges)),
        "repeats_per_fraction": next(iter(repeats)),
        "all_incremental_results_match_full_recompute": True,
        "crossover": {
            "observed": crossover is not None,
            "first_fraction_with_median_speedup_le_1": crossover,
            "conclusion": (
                f"median incremental speedup reached <=1 at update fraction {crossover}"
                if crossover is not None
                else "no crossover observed in the configured update-fraction sweep"
            ),
        },
        "fractions": fractions,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
