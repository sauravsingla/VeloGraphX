#!/usr/bin/env python3
"""Generate deterministic post-selector holdout graphs for reviewer-facing CI evidence.

These graphs are intentionally synthetic and are never promoted as substitutes for
publication datasets. Their purpose is to exercise a frozen selector on graph
families that were not used to tune publication-preflight-v1.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

MASK = (1 << 64) - 1
GENERATOR_VERSION = "reviewer-holdout-v1"


def splitmix64(x: int) -> int:
    x = (x + 0x9E3779B97F4A7C15) & MASK
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & MASK
    return (x ^ (x >> 31)) & MASK


def permute_edges(edges: set[tuple[int, int]], seed: int) -> list[tuple[int, int]]:
    def key(edge: tuple[int, int]) -> tuple[int, int, int]:
        u, v = edge
        packed = ((u & 0xFFFFFFFF) << 32) | (v & 0xFFFFFFFF)
        return splitmix64(packed ^ seed), u, v

    return sorted(edges, key=key)


def road_smallworld(width: int, height: int, seed: int) -> tuple[list[tuple[int, int]], dict]:
    n = width * height
    edges: set[tuple[int, int]] = set()

    def add_bidir(a: int, b: int) -> None:
        if a != b:
            edges.add((a, b))
            edges.add((b, a))

    for y in range(height):
        for x in range(width):
            u = y * width + x
            if x + 1 < width:
                add_bidir(u, u + 1)
            if y + 1 < height:
                add_bidir(u, u + width)

    shortcut_target = max(1, n // 4)
    state = seed & MASK
    added = 0
    while added < shortcut_target:
        state = splitmix64(state)
        u = state % n
        state = splitmix64(state)
        v = state % n
        if u == v or (u, v) in edges:
            continue
        # Avoid turning the graph into a dense random graph: retain a sparse,
        # road-like backbone with a small number of long directed shortcuts.
        ux, uy = u % width, u // width
        vx, vy = v % width, v // width
        if abs(ux - vx) + abs(uy - vy) < max(8, width // 12):
            continue
        edges.add((u, v))
        added += 1

    ordered = permute_edges(edges, seed ^ 0x524F4144)
    metadata = {
        "mode": "road-smallworld-directed",
        "directed": True,
        "vertices": n,
        "edges": len(ordered),
        "width": width,
        "height": height,
        "shortcuts": shortcut_target,
        "seed": seed,
        "roots": [0, (height // 2) * width + (width // 2), n - 1],
    }
    return ordered, metadata


def clustered_undirected(vertices: int, seed: int) -> tuple[list[tuple[int, int]], dict]:
    if vertices < 64:
        raise ValueError("clustered-undirected requires at least 64 vertices")
    edges: set[tuple[int, int]] = set()

    def add(a: int, b: int) -> None:
        if a == b:
            return
        if a > b:
            a, b = b, a
        edges.add((a, b))

    # Ring lattice creates stable local clustering and many triangles.
    for u in range(vertices):
        for delta in range(1, 5):
            add(u, (u + delta) % vertices)

    chord_target = vertices
    state = seed & MASK
    added = 0
    while added < chord_target:
        state = splitmix64(state)
        u = state % vertices
        state = splitmix64(state)
        v = state % vertices
        if u == v:
            continue
        before = len(edges)
        add(u, v)
        if len(edges) != before:
            added += 1

    ordered = permute_edges(edges, seed ^ 0x54524941)
    metadata = {
        "mode": "clustered-undirected",
        "directed": False,
        "vertices": vertices,
        "edges": len(ordered),
        "ring_neighbors_each_side": 4,
        "random_chords": chord_target,
        "seed": seed,
    }
    return ordered, metadata


def write_graph(path: Path, edges: list[tuple[int, int]]) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {GENERATOR_VERSION}\n")
        for u, v in edges:
            f.write(f"{u} {v}\n")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("road-smallworld", "clustered-undirected"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260914)
    parser.add_argument("--width", type=int, default=192)
    parser.add_argument("--height", type=int, default=192)
    parser.add_argument("--vertices", type=int, default=8192)
    args = parser.parse_args()

    if args.mode == "road-smallworld":
        edges, metadata = road_smallworld(args.width, args.height, args.seed)
    else:
        edges, metadata = clustered_undirected(args.vertices, args.seed)

    digest = write_graph(args.output, edges)
    metadata.update({
        "schema_version": 1,
        "artifact_type": "velographx-reviewer-holdout-graph",
        "generator_version": GENERATOR_VERSION,
        "sha256": digest,
        "output": str(args.output),
        "research_claim": False,
    })
    args.metadata.parent.mkdir(parents=True, exist_ok=True)
    args.metadata.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(metadata, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
