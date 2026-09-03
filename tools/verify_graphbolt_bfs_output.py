#!/usr/bin/env python3
"""Verify GraphBolt BFS reachability output against fresh recomputation."""

from __future__ import annotations

import argparse
import json
from collections import deque
from pathlib import Path


def edge_rows(path: Path):
    for number, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(("#", "%")):
            continue
        fields = line.split()
        if len(fields) < 2:
            raise ValueError(f"{path}:{number}: expected source and target")
        yield int(fields[0]), int(fields[1])


def apply_stream(edges: set[tuple[int, int]], path: Path):
    additions = deletions = 0
    for number, raw in enumerate(path.read_text().splitlines(), 1):
        fields = raw.split()
        if len(fields) < 3 or fields[0] not in {"a", "d"}:
            raise ValueError(f"{path}:{number}: expected 'a|d source target'")
        edge = (int(fields[1]), int(fields[2]))
        if fields[0] == "a":
            if edge in edges:
                raise ValueError(f"{path}:{number}: invalid duplicate addition")
            edges.add(edge); additions += 1
        else:
            if edge not in edges:
                raise ValueError(f"{path}:{number}: invalid deletion of absent edge")
            edges.remove(edge); deletions += 1
    return additions, deletions


def fresh_reachable(edges: set[tuple[int, int]], source: int):
    adjacency = {}
    vertices = {source}
    for u, v in edges:
        adjacency.setdefault(u, []).append(v)
        vertices.update((u, v))
    reached, queue = {source}, deque([source])
    while queue:
        for target in adjacency.get(queue.popleft(), ()):
            if target not in reached:
                reached.add(target); queue.append(target)
    return vertices, reached


def graphbolt_reachable(path: Path):
    values = {}
    for number, raw in enumerate(path.read_text().splitlines(), 1):
        fields = raw.split()
        if len(fields) < 3:
            raise ValueError(f"{path}:{number}: expected vertex, indegree, value")
        vertex, value = int(fields[0]), int(fields[-1])
        if vertex in values or value not in {0, 1}:
            raise ValueError(f"{path}:{number}: invalid or duplicate BFS value")
        values[vertex] = value
    return values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--initial", type=Path, required=True)
    parser.add_argument("--stream", type=Path, required=True)
    parser.add_argument("--graphbolt-output", type=Path, required=True)
    parser.add_argument("--source", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    edges = set(edge_rows(args.initial))
    additions, deletions = apply_stream(edges, args.stream)
    vertices, expected = fresh_reachable(edges, args.source)
    observed = graphbolt_reachable(args.graphbolt_output)
    if set(observed) != vertices:
        raise ValueError("GraphBolt vertex domain differs from fresh recomputation")
    mismatches = sorted(v for v in vertices if observed[v] != int(v in expected))
    if mismatches:
        raise ValueError(f"GraphBolt BFS reachability differs at {len(mismatches)} vertices")
    result = {
        "schema_version": 1, "artifact_type": "graphbolt-bfs-fresh-recompute-gate",
        "semantics": "directed unweighted reachability, matching GraphBolt apps/BFS.C 0/1 output",
        "source": args.source, "vertices": len(vertices), "final_edges": len(edges),
        "valid_additions": additions, "valid_deletions": deletions,
        "reachable_vertices": len(expected), "exact": True, "research_claim": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
