#!/usr/bin/env python3
"""Prepare an order-preserving integer edge stream for external baselines.

Unlike the generic normalizer, this helper deliberately does not sort, remap,
or deduplicate edges. Dynamic-system workloads often derive update windows from
input order, so the canonical source order is part of the benchmark semantics.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--report", required=True, type=Path)
    p.add_argument("--expected-vertices", required=True, type=int)
    p.add_argument("--expected-edges", required=True, type=int)
    args = p.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    rows = 0
    min_vertex = None
    max_vertex = None
    self_loops = 0
    with args.input.open("r", encoding="utf-8") as src, args.output.open(
        "w", encoding="utf-8"
    ) as dst:
        for lineno, raw in enumerate(src, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError(f"{args.input}:{lineno}: expected two integer columns")
            u, v = int(parts[0]), int(parts[1])
            if u < 0 or v < 0:
                raise ValueError(f"{args.input}:{lineno}: negative vertex id")
            rows += 1
            min_vertex = min(u, v) if min_vertex is None else min(min_vertex, u, v)
            max_vertex = max(u, v) if max_vertex is None else max(max_vertex, u, v)
            self_loops += int(u == v)
            dst.write(f"{u}\t{v}\n")

    if rows != args.expected_edges:
        raise RuntimeError(f"expected {args.expected_edges} edges, observed {rows}")
    if min_vertex != 0 or max_vertex != args.expected_vertices - 1:
        raise RuntimeError(
            f"expected vertex id range 0..{args.expected_vertices - 1}, "
            f"observed {min_vertex}..{max_vertex}"
        )

    report = {
        "schema_version": 1,
        "artifact_type": "velographx-order-preserving-external-edge-stream",
        "research_claim": False,
        "source_path": str(args.input),
        "source_sha256": sha256_file(args.input),
        "output_path": str(args.output),
        "output_sha256": sha256_file(args.output),
        "edge_count": rows,
        "expected_vertex_count": args.expected_vertices,
        "min_vertex_id": min_vertex,
        "max_vertex_id": max_vertex,
        "self_loop_rows": self_loops,
        "order_preserved": True,
        "sorted": False,
        "deduplicated": False,
        "remapped": False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
