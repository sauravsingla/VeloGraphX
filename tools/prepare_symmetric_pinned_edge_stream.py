#!/usr/bin/env python3
"""Prepare a deterministic simple symmetric edge stream from a pinned undirected source.

The source checksum is verified before parsing. Self-loops are counted and removed.
Non-loop rows are collapsed by unordered endpoint pair, with a caller-specified
expected multiplicity used to verify reciprocal/duplicate source representation.
Vertices are dense-relabelled by ascending original identifier, and every unique
undirected pair is emitted exactly once in each direction, ordered by first source
appearance.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
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
    p.add_argument("--expected-nonloop-pair-multiplicity", type=int, default=1)
    p.add_argument("--expected-unique-nonloop-pairs", type=int, default=None)
    args = p.parse_args()

    if args.expected_nonloop_pair_multiplicity <= 0:
        raise ValueError("expected non-loop pair multiplicity must be positive")

    source_hash = sha256(args.input)
    if source_hash != args.expected_sha256:
        raise RuntimeError(f"source SHA-256 {source_hash} != expected {args.expected_sha256}")

    source_rows = 0
    self_loops = 0
    vertices: set[int] = set()
    ordered_pairs: list[tuple[int, int]] = []
    pair_counts: Counter[tuple[int, int]] = Counter()
    orientation_counts: Counter[tuple[int, int]] = Counter()

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
            if pair_counts[key] == 0:
                ordered_pairs.append(key)
            pair_counts[key] += 1
            orientation_counts[(u, v)] += 1

    if source_rows != args.expected_source_rows:
        raise RuntimeError(f"source rows {source_rows} != expected {args.expected_source_rows}")
    if self_loops != args.expected_self_loops:
        raise RuntimeError(f"self-loop rows {self_loops} != expected {args.expected_self_loops}")
    if len(vertices) != args.expected_vertices:
        raise RuntimeError(f"vertices {len(vertices)} != expected {args.expected_vertices}")
    if args.expected_unique_nonloop_pairs is not None and len(pair_counts) != args.expected_unique_nonloop_pairs:
        raise RuntimeError(
            f"unique non-loop pairs {len(pair_counts)} != expected {args.expected_unique_nonloop_pairs}"
        )

    wrong_multiplicity = [
        (pair, count) for pair, count in pair_counts.items()
        if count != args.expected_nonloop_pair_multiplicity
    ]
    if wrong_multiplicity:
        sample = wrong_multiplicity[:5]
        raise RuntimeError(
            f"{len(wrong_multiplicity)} undirected pairs violate expected multiplicity "
            f"{args.expected_nonloop_pair_multiplicity}; sample={sample}"
        )

    reciprocal_pairs = 0
    if args.expected_nonloop_pair_multiplicity == 2:
        for a, b in pair_counts:
            if orientation_counts[(a, b)] != 1 or orientation_counts[(b, a)] != 1:
                raise RuntimeError(
                    f"pair {(a, b)} has multiplicity 2 but not one row in each orientation: "
                    f"{orientation_counts[(a, b)]}/{orientation_counts[(b, a)]}"
                )
            reciprocal_pairs += 1

    dense = {v: i for i, v in enumerate(sorted(vertices))}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as out:
        for u, v in ordered_pairs:
            du, dv = dense[u], dense[v]
            out.write(f"{du} {dv}\n")
            out.write(f"{dv} {du}\n")

    directed_edges = len(ordered_pairs) * 2
    output_hash = sha256(args.output)
    report = {
        "schema_version": 2,
        "artifact_type": "velographx-pinned-symmetric-edge-stream",
        "source_path": str(args.input),
        "source_sha256": source_hash,
        "source_rows": source_rows,
        "source_self_loop_rows_removed": self_loops,
        "source_nonloop_rows": source_rows - self_loops,
        "source_unique_nonloop_undirected_edges": len(ordered_pairs),
        "source_expected_nonloop_pair_multiplicity": args.expected_nonloop_pair_multiplicity,
        "source_reciprocal_pairs_verified": reciprocal_pairs,
        "vertex_count": len(vertices),
        "dense_min_vertex": 0,
        "dense_max_vertex": len(vertices) - 1,
        "output_path": str(args.output),
        "edge_count": directed_edges,
        "output_directed_edges": directed_edges,
        "output_sha256": output_hash,
        "preparation": (
            "checksum-pinned source; self-loops removed explicitly; non-loop rows collapsed by "
            "unordered pair with exact multiplicity/orientation verification; ascending-ID dense "
            "relabel; each unique undirected pair emitted once forward and once reverse in first-appearance order"
        ),
        "directed_representation_of_undirected_graph": True,
        "research_claim": False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
