#!/usr/bin/env python3
"""Prepare a deterministic simple symmetric edge stream from a pinned undirected source.

The source checksum is verified before parsing. Self-loops are counted and removed,
duplicate undirected pairs are rejected, vertices are dense-relabelled by ascending
original identifier, and each accepted undirected pair is emitted in both directions
consecutively while preserving source-row order.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    p.add_argument("--expected-sha256", required=True)
    p.add_argument("--expected-vertices", type=int, required=True)
    p.add_argument("--expected-source-rows", type=int, required=True)
    p.add_argument("--expected-self-loops", type=int, required=True)
    args = p.parse_args()

    source_hash = sha256(args.input)
    if source_hash != args.expected_sha256:
        raise RuntimeError(f"source SHA-256 {source_hash} != expected {args.expected_sha256}")

    source_rows = 0
    self_loops = 0
    vertices: set[int] = set()
    undirected: list[tuple[int, int]] = []
    seen: set[tuple[int, int]] = set()

    with args.input.open("r", encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError(f"{args.input}:{lineno}: expected at least two columns")
            u, v = int(parts[0]), int(parts[1])
            source_rows += 1
            vertices.add(u)
            vertices.add(v)
            if u == v:
                self_loops += 1
                continue
            key = (u, v) if u < v else (v, u)
            if key in seen:
                raise ValueError(f"{args.input}:{lineno}: duplicate undirected pair {key}")
            seen.add(key)
            undirected.append((u, v))

    if source_rows != args.expected_source_rows:
        raise RuntimeError(f"source rows {source_rows} != expected {args.expected_source_rows}")
    if self_loops != args.expected_self_loops:
        raise RuntimeError(f"self-loop rows {self_loops} != expected {args.expected_self_loops}")
    if len(vertices) != args.expected_vertices:
        raise RuntimeError(f"vertices {len(vertices)} != expected {args.expected_vertices}")

    dense = {v: i for i, v in enumerate(sorted(vertices))}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as out:
        for u, v in undirected:
            du, dv = dense[u], dense[v]
            out.write(f"{du} {dv}\n")
            out.write(f"{dv} {du}\n")

    directed_edges = len(undirected) * 2
    output_hash = sha256(args.output)
    report = {
        "schema_version": 1,
        "artifact_type": "velographx-pinned-symmetric-edge-stream",
        "source_path": str(args.input),
        "source_sha256": source_hash,
        "source_rows": source_rows,
        "source_self_loop_rows_removed": self_loops,
        "source_unique_nonloop_undirected_edges": len(undirected),
        "vertex_count": len(vertices),
        "dense_min_vertex": 0,
        "dense_max_vertex": len(vertices) - 1,
        "output_path": str(args.output),
        "edge_count": directed_edges,
        "output_directed_edges": directed_edges,
        "output_sha256": output_hash,
        "preparation": "checksum-pinned source; self-loops removed; duplicate undirected pairs rejected; ascending-ID dense relabel; each source-order undirected edge emitted forward then reverse",
        "directed_representation_of_undirected_graph": True,
        "research_claim": False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
