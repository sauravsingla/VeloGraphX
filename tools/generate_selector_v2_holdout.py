#!/usr/bin/env python3
"""Generate fresh post-v2 holdout graph families.

These generators are intentionally distinct from the road-smallworld and
clustered-ring families retained in the post-PR71 campaign.  They exist only to
check whether selector-v2 behavior transfers after the prior failures became
visible development evidence.  Hosted timings remain research_claim=false.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

MASK = (1 << 64) - 1
GENERATOR_VERSION = "selector-v2-holdout-v1"


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


def chain_hubs_directed(vertices: int, seed: int) -> tuple[list[tuple[int, int]], dict]:
    if vertices < 4096:
        raise ValueError("chain-hubs-directed requires at least 4096 vertices")
    edges: set[tuple[int, int]] = set()

    # Bidirectional chain gives high diameter without reusing the grid/road
    # topology from the previous holdout.
    for u in range(vertices - 1):
        edges.add((u, u + 1))
        edges.add((u + 1, u))

    # Sparse hub fan-out produces heterogeneous affected regions.
    stride = 256
    fanout = 12
    for hub in range(0, vertices, stride):
        for j in range(1, fanout + 1):
            v = min(vertices - 1, hub + j * (stride // 2))
            if hub != v:
                edges.add((hub, v))

    # A small deterministic long-edge set makes reachability changes less local
    # while keeping the graph sparse and structurally different from a grid.
    target = max(1, vertices // 8)
    state = seed & MASK
    added = 0
    while added < target:
        state = splitmix64(state)
        u = state % vertices
        state = splitmix64(state)
        v = state % vertices
        if u == v or abs(int(u) - int(v)) < stride:
            continue
        before = len(edges)
        edges.add((u, v))
        if len(edges) != before:
            added += 1

    ordered = permute_edges(edges, seed ^ 0x434841494E)
    roots = [0, vertices // 2, vertices - 1]
    return ordered, {
        "mode": "chain-hubs-directed",
        "directed": True,
        "vertices": vertices,
        "edges": len(ordered),
        "hub_stride": stride,
        "fanout_per_hub": fanout,
        "long_edges": target,
        "seed": seed,
        "roots": roots,
    }


def block_community_undirected(vertices: int, seed: int) -> tuple[list[tuple[int, int]], dict]:
    if vertices < 2048 or vertices % 64:
        raise ValueError("block-community-undirected requires vertices >= 2048 and divisible by 64")
    edges: set[tuple[int, int]] = set()
    community = 64

    def add(a: int, b: int) -> None:
        if a == b:
            return
        if a > b:
            a, b = b, a
        edges.add((a, b))

    # Each block has a deterministic locally dense pattern, but unlike the
    # previous ring-lattice generator the topology is community/block based.
    for base in range(0, vertices, community):
        for u in range(base, base + community):
            local = u - base
            for delta in (1, 2, 5, 11):
                add(u, base + ((local + delta) % community))
        # Deterministic spokes create triangles around block anchors.
        anchor = base
        for local in range(2, community, 3):
            add(anchor, base + local)

    # Sparse bridges connect adjacent and pseudo-random communities.
    communities = vertices // community
    for c in range(communities):
        a = c * community
        b = ((c + 1) % communities) * community
        add(a, b)
        add(a + 7, b + 13)

    state = seed & MASK
    target = communities * 4
    added = 0
    while added < target:
        state = splitmix64(state)
        c1 = state % communities
        state = splitmix64(state)
        c2 = state % communities
        if c1 == c2:
            continue
        state = splitmix64(state)
        u = c1 * community + (state % community)
        state = splitmix64(state)
        v = c2 * community + (state % community)
        before = len(edges)
        add(u, v)
        if len(edges) != before:
            added += 1

    ordered = permute_edges(edges, seed ^ 0x424C4F434B)
    return ordered, {
        "mode": "block-community-undirected",
        "directed": False,
        "vertices": vertices,
        "edges": len(ordered),
        "community_size": community,
        "communities": communities,
        "random_bridges": target,
        "seed": seed,
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
    parser.add_argument("--mode", choices=("chain-hubs-directed", "block-community-undirected"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=2026091501)
    parser.add_argument("--vertices", type=int, default=32768)
    args = parser.parse_args()

    if args.mode == "chain-hubs-directed":
        edges, metadata = chain_hubs_directed(args.vertices, args.seed)
    else:
        edges, metadata = block_community_undirected(args.vertices, args.seed)

    digest = write_graph(args.output, edges)
    metadata.update({
        "schema_version": 1,
        "artifact_type": "velographx-selector-v2-holdout-graph",
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
