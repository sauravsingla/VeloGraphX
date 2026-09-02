#!/usr/bin/env python3
"""Select fixed roots without consulting benchmark timings or outcomes."""

from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--edge-count", type=int, required=True)
    parser.add_argument("--base-rate", type=float, default=0.8)
    parser.add_argument("--maximum-fraction", type=float, default=0.1)
    parser.add_argument("--alignment", type=int, default=1)
    parser.add_argument("--count", type=int, default=3)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not (0 < args.base_rate < 1) or args.maximum_fraction <= 0 or args.alignment <= 0:
        raise SystemExit("invalid campaign dimensions")

    base = int(args.edge_count * args.base_rate)
    base -= base % args.alignment
    maximum_batch = math.ceil(base * args.maximum_fraction)
    maximum_batch += (-maximum_batch) % args.alignment
    if base + maximum_batch > args.edge_count:
        raise SystemExit("stream is too short for the base and maximum update fraction")

    initial: dict[int, int] = defaultdict(int)
    final: dict[int, int] = defaultdict(int)
    with args.input.open(encoding="utf-8") as handle:
        for index, line in enumerate(handle):
            if index >= base + maximum_batch:
                break
            u_text, _ = line.split()[:2]
            u = int(u_text)
            if index < base:
                initial[u] += 1
            if maximum_batch <= index < base + maximum_batch:
                final[u] += 1
    candidates = sorted(
        set(initial) & set(final),
        key=lambda u: (-min(initial[u], final[u]), -(initial[u] + final[u]), u),
    )
    roots = candidates[: args.count]
    if len(roots) != args.count:
        raise SystemExit("not enough roots with outgoing edges in both endpoint graphs")
    result = {
        "schema_version": 1,
        "selection_policy": "highest minimum initial/final out-degree, then total degree, then vertex id",
        "timing_or_result_data_consulted": False,
        "base_edges": base,
        "maximum_batch_edges": maximum_batch,
        "alignment": args.alignment,
        "roots": roots,
        "root_degrees": [
            {"root": root, "initial_out_degree": initial[root], "maximum_fraction_final_out_degree": final[root]}
            for root in roots
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
