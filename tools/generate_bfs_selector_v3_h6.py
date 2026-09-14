#!/usr/bin/env python3
"""Generate the preregistered selector-v3 H6 BFS holdout graph.

The layered-diamond family is intentionally distinct from the previously observed
road/grid, chain-hubs, clustered-ring, and block-community families. The generator
is deterministic; hosted timings remain research_claim=false.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

MASK = (1 << 64) - 1
GENERATOR_VERSION = "bfs-selector-v3-h6-v1"


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


def layered_diamond_directed(
    vertices: int, width: int, seed: int
) -> tuple[list[tuple[int, int]], dict]:
    if vertices < 8192 or width < 64 or vertices % width:
        raise ValueError(
            "layered-diamond-directed requires vertices >= 8192, width >= 64, "
            "and vertices divisible by width"
        )

    layers = vertices // width
    edges: set[tuple[int, int]] = set()

    # Every layer has a directed cycle plus sparse reverse-local chords. This
    # keeps roots reachable while retaining direction-sensitive local structure.
    for layer in range(layers):
        base = layer * width
        for i in range(width):
            u = base + i
            edges.add((u, base + ((i + 1) % width)))
            if i % 8 == 0:
                edges.add((u, base + ((i - 7) % width)))

    # Adjacent layers form overlapping directed diamonds. Sparse reverse edges
    # and a bidirectional anchor spine create heterogeneous deletion cascades
    # without reusing the chain-hub or road/grid topology.
    for layer in range(layers - 1):
        base = layer * width
        nxt = (layer + 1) * width
        for i in range(width):
            u = base + i
            edges.add((u, nxt + i))
            edges.add((u, nxt + ((i + 17) % width)))
            if i % 16 == 0:
                edges.add((nxt + i, u))
        edges.add((base, nxt))
        edges.add((nxt, base))

    # Deterministic long inter-layer edges create non-local alternatives while
    # excluding nearby layers so they remain structurally meaningful.
    state = seed & MASK
    target = max(1, vertices // 6)
    added = 0
    while added < target:
        state = splitmix64(state)
        l1 = state % layers
        state = splitmix64(state)
        l2 = state % layers
        if l1 == l2 or abs(int(l1) - int(l2)) < 8:
            continue
        state = splitmix64(state)
        i = state % width
        state = splitmix64(state)
        j = state % width
        u = l1 * width + i
        v = l2 * width + j
        before = len(edges)
        edges.add((u, v))
        if len(edges) != before:
            added += 1

    ordered = permute_edges(edges, seed ^ 0x484F4C4436)
    roots = [0, (layers // 2) * width + width // 3, vertices - 1]
    return ordered, {
        "mode": "layered-diamond-directed",
        "directed": True,
        "vertices": vertices,
        "edges": len(ordered),
        "layer_width": width,
        "layers": layers,
        "long_edges": target,
        "seed": seed,
        "roots": roots,
    }


def write_graph(path: Path, edges: list[tuple[int, int]]) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {GENERATOR_VERSION}\n")
        for u, v in edges:
            f.write(f"{u} {v}\n")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=2026091406)
    parser.add_argument("--vertices", type=int, default=49152)
    parser.add_argument("--width", type=int, default=256)
    args = parser.parse_args()

    edges, metadata = layered_diamond_directed(args.vertices, args.width, args.seed)
    digest = write_graph(args.output, edges)
    metadata.update(
        {
            "schema_version": 1,
            "artifact_type": "velographx-bfs-selector-v3-h6-graph",
            "generator_version": GENERATOR_VERSION,
            "sha256": digest,
            "output": str(args.output),
            "research_claim": False,
        }
    )
    args.metadata.parent.mkdir(parents=True, exist_ok=True)
    args.metadata.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(metadata, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
