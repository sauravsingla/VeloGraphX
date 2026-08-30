#!/usr/bin/env python3
"""Matched dynamic-BFS runner for NetworKit.

The contract mirrors benchmarks/external_risgraph_bfs.cpp:
- directed, unweighted graph
- checksum-normalized source-order edge stream
- import the first imported_rate fraction
- for each remaining batch, add the new source-order edges and remove the
  equally sized oldest source-order edges (sliding window)
- restore an exact single-source BFS answer after every batch

Timing includes graph mutation plus NetworKit DynBFS::updateBatch. Full BFS
verification is executed outside the timed region after every batch.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import time
from pathlib import Path

import networkit as nk
import numpy as np


def read_edges(path: Path) -> np.ndarray:
    rows = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            if not line or line[0] == "#":
                continue
            parts = line.split()
            if len(parts) >= 2:
                rows.append((int(parts[0]), int(parts[1])))
    if not rows:
        raise RuntimeError("edge list is empty")
    return np.asarray(rows, dtype=np.int64)


def distance_vector(algorithm, n: int) -> list[int]:
    distances = algorithm.getDistances()
    if len(distances) != n:
        raise RuntimeError(f"distance-vector length {len(distances)} != {n}")
    out: list[int] = []
    for value in distances:
        if math.isinf(value) or value >= 1e300:
            out.append(-1)
        else:
            out.append(int(value))
    return out


def layer_counts(distances: list[int]) -> list[int]:
    max_depth = max((d for d in distances if d >= 0), default=0)
    counts = [0] * (max_depth + 1)
    for d in distances:
        if d >= 0:
            counts[d] += 1
    return counts


def digest_distances(distances: list[int]) -> str:
    h = hashlib.sha256()
    for d in distances:
        h.update(int(d).to_bytes(8, byteorder="little", signed=True))
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("edge_list", type=Path)
    parser.add_argument("root", type=int)
    parser.add_argument("imported_rate", type=float)
    parser.add_argument("batch_size", type=int)
    parser.add_argument("--expected-version", default="11.2.1")
    args = parser.parse_args()

    if not 0.0 < args.imported_rate < 1.0:
        raise ValueError("imported_rate must be in (0, 1)")
    if args.batch_size <= 0:
        raise ValueError("batch_size must be positive")
    if nk.__version__ != args.expected_version:
        raise RuntimeError(
            f"NetworKit version {nk.__version__!r} != pinned {args.expected_version!r}"
        )

    edges = read_edges(args.edge_list)
    n = int(edges.max()) + 1
    m = int(edges.shape[0])
    imported = int(m * args.imported_rate)
    if imported <= 0 or imported >= m:
        raise RuntimeError("imported rate leaves no streaming window")
    if args.root < 0 or args.root >= n:
        raise RuntimeError("root is outside graph domain")

    initial = edges[:imported]
    graph = nk.GraphFromCoo(
        (initial[:, 0], initial[:, 1]), n=n, weighted=False, directed=True
    )
    if graph.numberOfEdges() != imported:
        raise RuntimeError(
            "initial graph edge count differs from source stream; duplicate edges "
            "would invalidate the matched simple-graph contract"
        )

    dyn = nk.distance.DynBFS(graph, args.root)
    dyn.run()

    total_timed_ns = 0
    mutation_ns = 0
    maintenance_ns = 0
    batches = 0
    operations = 0
    batch_correctness: list[bool] = []
    batch_digests: list[str] = []

    event_add = nk.dynamics.GraphEventType.EDGE_ADDITION
    event_remove = nk.dynamics.GraphEventType.EDGE_REMOVAL

    for local_begin in range(imported, m, args.batch_size):
        local_end = min(local_begin + args.batch_size, m)
        add_edges = edges[local_begin:local_end]
        remove_begin = local_begin - imported
        remove_end = local_end - imported
        remove_edges = edges[remove_begin:remove_end]

        events = []
        start = time.perf_counter_ns()
        mutation_start = start
        for u, v in add_edges:
            graph.addEdge(int(u), int(v), checkMultiEdge=True)
            events.append(nk.dynamics.GraphEvent(event_add, int(u), int(v), 1.0))
        for u, v in remove_edges:
            graph.removeEdge(int(u), int(v))
            events.append(nk.dynamics.GraphEvent(event_remove, int(u), int(v), 1.0))
        mutation_end = time.perf_counter_ns()

        maintenance_start = mutation_end
        dyn.updateBatch(events)
        end = time.perf_counter_ns()

        total_timed_ns += end - start
        mutation_ns += mutation_end - mutation_start
        maintenance_ns += end - maintenance_start
        operations += len(events)
        batches += 1

        maintained = distance_vector(dyn, n)
        full = nk.distance.BFS(graph, args.root, storePaths=False)
        full.run()
        reference = distance_vector(full, n)
        correct = maintained == reference
        batch_correctness.append(correct)
        batch_digests.append(digest_distances(maintained))
        if not correct:
            raise RuntimeError(f"NetworKit DynBFS mismatch after batch {batches}")

    final_distances = distance_vector(dyn, n)
    layers = layer_counts(final_distances)
    visited = sum(layers)
    final_edges = int(graph.numberOfEdges())
    if final_edges != imported:
        raise RuntimeError(f"final edge count {final_edges} != initial window {imported}")

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-external-networkit-dynbfs-baseline",
        "networkit_version": nk.__version__,
        "networkit_git_revision": "359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c",
        "algorithm": "networkit.distance.DynBFS",
        "directed": True,
        "weighted": False,
        "vertices": n,
        "source_edges": m,
        "initial_edges": imported,
        "final_edges": final_edges,
        "root": args.root,
        "imported_rate": args.imported_rate,
        "batch_size": args.batch_size,
        "batches": batches,
        "update_operations": operations,
        "update_semantics": (
            "per batch: add next source-order window edges, remove equally many "
            "oldest source-order window edges, then maintain exact BFS"
        ),
        "timing_envelope": "graph mutation plus DynBFS.updateBatch; correctness BFS excluded",
        "wall_total_us": total_timed_ns / 1000.0,
        "wall_mean_us": (total_timed_ns / 1000.0 / batches) if batches else 0.0,
        "mutation_total_us": mutation_ns / 1000.0,
        "maintenance_total_us": maintenance_ns / 1000.0,
        "all_batches_correct_against_fresh_full_bfs": all(batch_correctness),
        "correctness_batches": len(batch_correctness),
        "final_distance_sha256": digest_distances(final_distances),
        "batch_distance_sha256": batch_digests,
        "visited_vertices": visited,
        "layer_counts": layers,
        "hosted_ci_single_execution": True,
        "research_claim": False,
    }
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
