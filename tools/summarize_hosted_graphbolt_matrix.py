#!/usr/bin/env python3
"""Summarize hosted VeloGraphX/GraphBolt/GAPBS evidence without hiding losses."""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
from pathlib import Path


def stats(values):
    if not values:
        return None
    values = [float(v) for v in values]
    median = statistics.median(values)
    mad = statistics.median(abs(v - median) for v in values)
    mean = statistics.mean(values)
    stdev = statistics.pstdev(values) if len(values) > 1 else 0.0
    return {
        "samples": values,
        "n": len(values),
        "min": min(values),
        "max": max(values),
        "mean": mean,
        "median": median,
        "mad": mad,
        "stdev": stdev,
        "cv": (stdev / mean) if mean else 0.0,
        "sub_millisecond_median": median < 1000.0,
    }


def json_files(path: Path, prefix: str):
    """Load only numbered sample files for a prefix.

    Exact matching is important because ``gb-*.json`` would also select
    ``gb-exact-*.json`` and silently mix parser output with verifier output.
    """
    pattern = re.compile(rf"^{re.escape(prefix)}-(\d+)\.json$")
    matches = []
    for candidate in path.iterdir():
        match = pattern.fullmatch(candidate.name)
        if match:
            matches.append((int(match.group(1)), candidate))
    return [json.loads(candidate.read_text()) for _, candidate in sorted(matches)]


def gap_samples(path: Path):
    text = path.read_text() if path.exists() else ""
    return [float(x) * 1e6 for x in re.findall(r"Trial Time:\s*([0-9.]+)", text)]


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset-dir", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--expected-samples", type=int, default=7)
    args = p.parse_args()

    dataset_meta = json.loads((args.dataset_dir / "dataset-metadata.json").read_text())
    rows = []
    dynamic_wins = {"velographx": 0, "graphbolt": 0, "tie": 0}
    noise_rows = 0
    rows_with_graphbolt_valid_counts = 0
    rows_with_graphbolt_edgework = 0

    for regime in sorted(p for p in args.dataset_dir.iterdir() if p.is_dir()):
        meta_path = regime / "metadata.json"
        if not meta_path.exists():
            continue
        workload = json.loads(meta_path.read_text())
        operations = workload["split"]["graphbolt_operation_rows"]
        expected_additions = workload["split"]["insertion_rows"]
        expected_deletions = workload["split"]["deletion_rows"]
        for root_dir in sorted(p for p in regime.glob("root-*") if p.is_dir()):
            root = int(root_dir.name.split("-", 1)[1])
            vx = json_files(root_dir, "vx")
            gb = json_files(root_dir, "gb")
            gx = json_files(root_dir, "gb-exact")
            if not (len(vx) == len(gb) == len(gx) == args.expected_samples):
                raise ValueError(
                    f"{root_dir}: expected {args.expected_samples} samples per dynamic system; "
                    f"found vx={len(vx)} gb={len(gb)} gb_exact={len(gx)}"
                )
            if not all(x.get("exact") for x in vx):
                raise ValueError(f"{root_dir}: VeloGraphX exactness failure")
            if not all(x.get("exact") for x in gx):
                raise ValueError(f"{root_dir}: GraphBolt exactness failure")

            vx_us = stats([x["answer_ready_us"] for x in vx])
            gb_us = stats([x["batches"][0]["answer_ready_seconds"] * 1e6 for x in gb])
            gb_mut = stats([x["batches"][0]["mutation_seconds"] * 1e6 for x in gb])
            gb_compute = stats([x["batches"][0]["compute_seconds"] * 1e6 for x in gb])
            gb_read = stats([x["batches"][0]["reading_seconds"] * 1e6 for x in gb])
            gap_us = stats(gap_samples(root_dir / "gapbs.log"))
            if gap_us is None or gap_us["n"] != args.expected_samples:
                found = 0 if gap_us is None else gap_us["n"]
                raise ValueError(f"{root_dir}: expected {args.expected_samples} GAPBS samples; found {found}")

            work_values = [x["batches"][0].get("edges_processed") for x in gb]
            work_values = [x for x in work_values if x is not None]
            graphbolt_work = stats(work_values) if work_values else None
            graphbolt_edgework_available = bool(work_values)
            if graphbolt_edgework_available:
                rows_with_graphbolt_edgework += 1

            affected = stats([x["affected_vertices"] for x in vx])
            additions = [x["batches"][0].get("additions", 0) for x in gb]
            deletions = [x["batches"][0].get("deletions", 0) for x in gb]
            reported_counts = [a + d for a, d in zip(additions, deletions)]
            count_availability = [count > 0 for count in reported_counts]
            if any(count_availability) and not all(count_availability):
                raise ValueError(f"{root_dir}: GraphBolt valid-operation counters are inconsistently emitted across samples")
            graphbolt_valid_counts_available = all(count_availability)
            if graphbolt_valid_counts_available:
                if any(count != operations for count in reported_counts):
                    raise ValueError(f"{root_dir}: GraphBolt reported valid-operation count mismatch")
                if any(a != expected_additions or d != expected_deletions for a, d in zip(additions, deletions)):
                    raise ValueError(f"{root_dir}: GraphBolt reported add/delete split mismatch")
                rows_with_graphbolt_valid_counts += 1

            vx_median = vx_us["median"]
            gb_median = gb_us["median"]
            if math.isclose(vx_median, gb_median, rel_tol=1e-12, abs_tol=1e-9):
                winner = "tie"
            elif vx_median < gb_median:
                winner = "velographx"
            else:
                winner = "graphbolt"
            dynamic_wins[winner] += 1
            noise = vx_us["sub_millisecond_median"] or gb_us["sub_millisecond_median"] or gap_us["sub_millisecond_median"]
            noise_rows += int(noise)

            rows.append({
                "regime": regime.name,
                "root": root,
                "initial_edges": workload["split"]["initial_rows"],
                "operations": operations,
                "operation_fraction_requested": workload["split"].get("operation_fraction_requested"),
                "operation_fraction_actual": workload["split"]["operation_fraction_actual"],
                "stream_additions_expected": expected_additions,
                "stream_deletions_expected": expected_deletions,
                "graphbolt_valid_operation_counts_available": graphbolt_valid_counts_available,
                "graphbolt_reported_additions": additions[0] if graphbolt_valid_counts_available else None,
                "graphbolt_reported_deletions": deletions[0] if graphbolt_valid_counts_available else None,
                "velographx_answer_ready_us": vx_us,
                "velographx_affected_vertices": affected,
                "graphbolt_answer_ready_us": gb_us,
                "graphbolt_mutation_us": gb_mut,
                "graphbolt_compute_us": gb_compute,
                "graphbolt_stream_read_us_excluded_from_answer_ready": gb_read,
                "graphbolt_edges_processed_available": graphbolt_edgework_available,
                "graphbolt_edges_processed": graphbolt_work,
                "gapbs_post_update_bfs_us": gap_us,
                "velographx_updates_per_second_at_median": operations / (vx_median / 1e6),
                "graphbolt_updates_per_second_at_median": operations / (gb_median / 1e6),
                "velographx_us_per_update_at_median": vx_median / operations,
                "graphbolt_us_per_update_at_median": gb_median / operations,
                "dynamic_comparable_winner": winner,
                "velographx_speedup_over_graphbolt": gb_median / vx_median,
                "noise_floor_flag": noise,
                "exact": True,
            })

    result = {
        "schema_version": 2,
        "artifact_type": "hosted-graphbolt-dzig-gapbs-expanded-matrix",
        "dataset": dataset_meta,
        "sample_count_per_root_regime": args.expected_samples,
        "statistics": "raw samples + median + MAD + population stdev + CV",
        "noise_floor_rule": "flag row when any reported system median is below 1000 us",
        "timing_scope": {
            "velographx": "graph mutation + incremental answer maintenance; verification excluded",
            "graphbolt": "graph mutation + incremental answer maintenance; stream read excluded and reported separately",
            "gapbs": "post-update BFS kernel only on already-materialized final graph",
        },
        "graphbolt_optional_metrics": {
            "valid_operation_counts": "reported only when the native artifact emits Affected add/delete counters; otherwise null, never inferred from zero",
            "edgework": "reported only when the native artifact emits EDGEWORK; otherwise null",
        },
        "dynamic_comparable_wins": dynamic_wins,
        "rows_with_noise_floor_flag": noise_rows,
        "rows_with_graphbolt_valid_operation_counts": rows_with_graphbolt_valid_counts,
        "rows_with_graphbolt_edgework": rows_with_graphbolt_edgework,
        "row_count": len(rows),
        "rows": rows,
        "publication_grade": False,
        "research_claim": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
