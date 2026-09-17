#!/usr/bin/env python3
"""Summarize matched VeloGraphX/GraphBolt exact dynamic-BFS executions.

This script reports only within-campaign, dimensionless paired ratios and medians.
It never combines absolute timings from unrelated runners/campaigns.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from statistics import mean, median


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--vx-glob", required=True)
    ap.add_argument("--graphbolt-glob", required=True)
    args = ap.parse_args()

    vx_paths = sorted(Path().glob(args.vx_glob))
    gb_paths = sorted(Path().glob(args.graphbolt_glob))
    if not vx_paths or not gb_paths:
        raise SystemExit("missing comparison results")

    def key(path: Path):
        # Expected stem: <system>__<fraction-tag>__<rep>
        parts = path.stem.split("__")
        if len(parts) != 3:
            raise ValueError(f"unexpected result name: {path}")
        return parts[1], int(parts[2])

    vx = {}
    all_exact = True
    for path in vx_paths:
        d = json.loads(path.read_text())
        all_exact = all_exact and bool(d.get("exact"))
        vx[key(path)] = float(d["answer_ready_us"])

    gb = {}
    for path in gb_paths:
        d = json.loads(path.read_text())
        batches = d.get("batches", [])
        if len(batches) != 1:
            raise ValueError(f"expected one GraphBolt batch in {path}")
        gb[key(path)] = float(batches[0]["answer_ready_seconds"]) * 1e6

    if set(vx) != set(gb):
        raise SystemExit(f"pair mismatch: vx-only={set(vx)-set(gb)}, gb-only={set(gb)-set(vx)}")

    rows = []
    for tag in sorted({k[0] for k in vx}):
        keys = sorted(k for k in vx if k[0] == tag)
        vx_us = [vx[k] for k in keys]
        gb_us = [gb[k] for k in keys]
        ratios = [gb[k] / max(1e-12, vx[k]) for k in keys]
        rows.append({
            "operation_fraction_tag": tag,
            "paired_repetitions": len(keys),
            "velographx_median_answer_ready_us": median(vx_us),
            "graphbolt_median_answer_ready_us": median(gb_us),
            "paired_graphbolt_over_velographx_ratio_mean": mean(ratios),
            "paired_graphbolt_over_velographx_ratio_median": median(ratios),
            "interpretation": (
                "ratio > 1 means lower VeloGraphX latency; ratio < 1 means lower GraphBolt latency"
            ),
        })

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-graphbolt-matched-dynamic-bfs-summary",
        "root": int(args.root),
        "all_velographx_results_exact": all_exact,
        "graphbolt_exactness": "verified independently for every retained paired run by workflow",
        "timing_boundary": "single post-update answer-ready batch; verification excluded from timed region",
        "scope": (
            "same GitHub-hosted machine allocation and matched mutation stream; GraphBolt executes in its "
            "pinned legacy Ubuntu 18.04/g++7/Cilk+/mimalloc runtime while VeloGraphX executes natively. "
            "This is scoped hosted comparative evidence, not a universal peak-performance claim."
        ),
        "rows": rows,
    }
    Path(args.output).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, sort_keys=True))
    if not all_exact:
        raise SystemExit("VeloGraphX exactness failed")


if __name__ == "__main__":
    main()
