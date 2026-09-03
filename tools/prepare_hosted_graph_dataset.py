#!/usr/bin/env python3
"""Prepare a bounded, provenance-recorded graph for hosted cross-system CI.

Real sources are sampled by the smallest seeded SHA-256 edge keys so selection is
independent of source row order. Vertex IDs are then remapped to a contiguous
range. Undirected inputs are expanded symmetrically before sampling. A small
R-MAT generator is provided for the synthetic family.
"""

from __future__ import annotations

import argparse
import hashlib
import heapq
import json
import random
from collections import Counter
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def key_for(seed: int, u: int, v: int) -> int:
    return int.from_bytes(hashlib.sha256(f"{seed}:{u}:{v}".encode()).digest(), "big")


def keep_smallest(heap, limit: int, seed: int, u: int, v: int) -> None:
    if u == v:
        return
    key = key_for(seed, u, v)
    item = (-key, u, v)
    if len(heap) < limit:
        heapq.heappush(heap, item)
    elif key < -heap[0][0]:
        heapq.heapreplace(heap, item)


def sample_real(path: Path, directed: bool, limit: int, seed: int):
    heap = []
    rows = 0
    with path.open("r", encoding="utf-8") as f:
        for line_no, raw in enumerate(f, 1):
            row = raw.strip()
            if not row or row.startswith(("#", "%")):
                continue
            fields = row.split()
            if len(fields) < 2:
                raise ValueError(f"line {line_no}: expected two integer columns")
            u, v = int(fields[0]), int(fields[1])
            rows += 1
            keep_smallest(heap, limit, seed, u, v)
            if not directed:
                keep_smallest(heap, limit, seed, v, u)
    if not heap:
        raise ValueError("source graph contains no usable edges")
    selected = {(u, v) for _, u, v in heap}
    return selected, rows


def rmat_vertex(rng: random.Random, scale: int):
    u = v = 0
    for bit in range(scale - 1, -1, -1):
        x = rng.random()
        if x < 0.57:
            q = 0
        elif x < 0.76:
            q = 1
        elif x < 0.95:
            q = 2
        else:
            q = 3
        if q in (2, 3):
            u |= 1 << bit
        if q in (1, 3):
            v |= 1 << bit
    return u, v


def sample_rmat(scale: int, limit: int, seed: int):
    rng = random.Random(seed)
    edges = set()
    attempts = 0
    max_attempts = limit * 50
    while len(edges) < limit and attempts < max_attempts:
        attempts += 1
        u, v = rmat_vertex(rng, scale)
        if u != v:
            edges.add((u, v))
    if len(edges) < limit:
        raise ValueError(f"R-MAT generator produced only {len(edges)} unique edges")
    return edges, attempts


def remap(edges):
    vertices = sorted({x for edge in edges for x in edge})
    mapping = {old: new for new, old in enumerate(vertices)}
    return sorted((mapping[u], mapping[v]) for u, v in edges), vertices


def choose_roots(edges, count: int = 3):
    degree = Counter(u for u, _ in edges)
    roots = [v for v, _ in sorted(degree.items(), key=lambda x: (-x[1], x[0]))[:count]]
    if len(roots) < count:
        candidates = sorted({x for edge in edges for x in edge})
        for v in candidates:
            if v not in roots:
                roots.append(v)
            if len(roots) == count:
                break
    return roots


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--metadata", type=Path, required=True)
    p.add_argument("--seed", type=int, default=20260903)
    p.add_argument("--max-edges", type=int, default=250000)
    p.add_argument("--input", type=Path)
    p.add_argument("--directed", action="store_true")
    p.add_argument("--source-name")
    p.add_argument("--source-family")
    p.add_argument("--rmat-scale", type=int)
    args = p.parse_args()
    if args.max_edges < 100:
        p.error("--max-edges must be >= 100")
    if bool(args.input) == bool(args.rmat_scale):
        p.error("provide exactly one of --input or --rmat-scale")

    if args.input:
        selected, source_rows = sample_real(args.input, args.directed, args.max_edges, args.seed)
        source = {
            "kind": "real",
            "path": str(args.input),
            "sha256": sha256(args.input),
            "rows_read": source_rows,
            "input_directed": args.directed,
        }
    else:
        selected, attempts = sample_rmat(args.rmat_scale, args.max_edges, args.seed)
        source = {
            "kind": "synthetic-rmat",
            "scale": args.rmat_scale,
            "attempts": attempts,
            "seed": args.seed,
        }

    normalized, old_vertices = remap(selected)
    roots = choose_roots(normalized, 3)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as f:
        for u, v in normalized:
            f.write(f"{u} {v}\n")

    meta = {
        "schema_version": 1,
        "artifact_type": "hosted-graphbolt-dataset",
        "name": args.source_name,
        "family": args.source_family,
        "selection": {
            "algorithm": "smallest seeded sha256(edge) keys" if args.input else "deterministic R-MAT",
            "seed": args.seed,
            "max_directed_edges": args.max_edges,
        },
        "source": source,
        "normalized": {
            "vertices": len(old_vertices),
            "directed_edges": len(normalized),
            "sha256": sha256(args.output),
            "contiguous_vertex_ids": True,
            "roots": roots,
        },
        "publication_grade": False,
        "research_claim": False,
    }
    args.metadata.parent.mkdir(parents=True, exist_ok=True)
    args.metadata.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n")
    print(json.dumps(meta, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
