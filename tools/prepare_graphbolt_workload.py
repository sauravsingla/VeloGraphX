#!/usr/bin/env python3
"""Create a deterministic, checksum-pinned initial graph and mutation stream.

Rows are ordered by a seeded cryptographic key, using an external merge so the
tool does not need to hold a publication-scale edge list in memory.
"""

from __future__ import annotations

import argparse
import hashlib
import heapq
import json
import sqlite3
import tempfile
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def keyed_rows(source: Path, seed: int, database: sqlite3.Connection):
    with source.open("r", encoding="utf-8") as handle:
        for line_number, raw in enumerate(handle, 1):
            stripped = raw.strip()
            if not stripped or stripped.startswith(("#", "%")):
                continue
            fields = stripped.split()
            if len(fields) < 2:
                raise ValueError(f"line {line_number}: expected at least two columns")
            source_vertex, target_vertex = int(fields[0]), int(fields[1])
            try:
                database.execute("INSERT INTO edges VALUES (?, ?)", (source_vertex, target_vertex))
            except sqlite3.IntegrityError as exc:
                raise ValueError(f"line {line_number}: duplicate directed edge") from exc
            normalized = " ".join(fields) + "\n"
            key = hashlib.sha256(f"{seed}:{line_number}:".encode() + normalized.encode()).hexdigest()
            yield key, line_number, normalized


def flush_chunk(rows, directory: Path, index: int) -> Path:
    rows.sort()
    path = directory / f"chunk-{index:06d}.txt"
    with path.open("w", encoding="utf-8") as handle:
        for key, line_number, row in rows:
            handle.write(f"{key}\t{line_number}\t{row}")
    return path


def read_chunk(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            key, line_number, row = line.split("\t", 2)
            yield key, int(line_number), row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260902)
    parser.add_argument("--initial-fraction", type=float, default=0.8)
    parser.add_argument("--chunk-rows", type=int, default=500_000)
    args = parser.parse_args()
    if not 0.0 < args.initial_fraction < 1.0 or args.chunk_rows < 1:
        parser.error("initial-fraction must be in (0,1) and chunk-rows must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="vx-shuffle-") as temp_name:
        temp = Path(temp_name)
        database = sqlite3.connect(temp / "edges.sqlite")
        database.execute("CREATE TABLE edges (source INTEGER, target INTEGER, PRIMARY KEY(source, target))")
        chunks, rows, total = [], [], 0
        for record in keyed_rows(args.input, args.seed, database):
            rows.append(record); total += 1
            if len(rows) >= args.chunk_rows:
                chunks.append(flush_chunk(rows, temp, len(chunks))); rows = []
        if rows:
            chunks.append(flush_chunk(rows, temp, len(chunks)))
        if total < 2:
            raise ValueError("at least two data rows are required")
        initial_count = max(1, min(total - 1, int(total * args.initial_fraction)))
        initial = args.output_dir / "initial.edges"
        insertions = args.output_dir / "insertions.edges"
        shuffled = args.output_dir / "shuffled.edges"
        with initial.open("w") as base, insertions.open("w") as updates, shuffled.open("w") as all_rows:
            merged = heapq.merge(*(read_chunk(path) for path in chunks))
            for index, (_, _, row) in enumerate(merged):
                all_rows.write(row)
                (base if index < initial_count else updates).write(row)

    # Valid deletions are sampled from the initial graph in its frozen order.
    deletions = args.output_dir / "deletions.edges"
    delete_count = min(total - initial_count, initial_count)
    with initial.open() as source, deletions.open("w") as target:
        for index, row in enumerate(source):
            if index == delete_count:
                break
            target.write(row)
    graphbolt_stream = args.output_dir / "graphbolt.stream"
    with insertions.open() as additions, deletions.open() as removals, graphbolt_stream.open("w") as target:
        for addition, deletion in zip(additions, removals):
            target.write("a " + addition)
            target.write("d " + deletion)
    files = {p.name: {"sha256": sha256(p), "rows": sum(1 for _ in p.open())}
             for p in (initial, insertions, deletions, graphbolt_stream, shuffled)}
    metadata = {
        "schema_version": 1,
        "artifact_type": "graphbolt-neutral-workload",
        "source": {"path": str(args.input), "sha256": sha256(args.input)},
        "shuffle": {"algorithm": "sha256(seed:source_line:normalized_row) external merge", "seed": args.seed},
        "split": {"initial_fraction_requested": args.initial_fraction, "initial_rows": initial_count,
                  "insertion_rows": total - initial_count, "deletion_rows": delete_count,
                  "graphbolt_operation_rows": delete_count * 2},
        "semantics": {"source_is_validated_as_a_simple_directed_edge_set": True,
                      "insertions_are_absent_from_initial_by_construction": True,
                      "deletions_are_present_in_initial_by_construction": True},
        "graphbolt": {
            "stream_format": "alternating 'a source target' and 'd source target' rows",
            "required_flags": ["-fixedBatchSize", "-enforceEdgeValidity", "-simple"],
            "batch_size_unit": "edge operations",
        },
        "files": files,
        "research_claim": False,
    }
    (args.output_dir / "metadata.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n")
    print(json.dumps(metadata, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
