#!/usr/bin/env python3
"""Prepare a simple directed edge stream for matched dynamic-BFS runs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--symmetrize", action="store_true")
    parser.add_argument("--drop-self-loops", action="store_true")
    parser.add_argument("--expected-source-sha256")
    args = parser.parse_args()

    source_sha = sha256_file(args.input)
    if args.expected_source_sha256 and source_sha != args.expected_source_sha256:
        raise SystemExit(
            f"source SHA-256 mismatch: expected {args.expected_source_sha256}, got {source_sha}"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    source_rows = output_rows = self_loops = 0
    maximum = -1
    with args.input.open(encoding="utf-8") as source, args.output.open("wb") as output:
        for line_number, line in enumerate(source, 1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            fields = stripped.split()
            if len(fields) < 2:
                raise SystemExit(f"invalid edge row at line {line_number}")
            u, v = int(fields[0]), int(fields[1])
            if u < 0 or v < 0 or u >= 2**32 or v >= 2**32:
                raise SystemExit(f"vertex outside uint32 range at line {line_number}")
            source_rows += 1
            maximum = max(maximum, u, v)
            if u == v:
                self_loops += 1
                if args.drop_self_loops:
                    continue
            rows = ((u, v), (v, u)) if args.symmetrize and u != v else ((u, v),)
            for left, right in rows:
                encoded = f"{left} {right}\n".encode()
                output.write(encoded)
                digest.update(encoded)
                output_rows += 1

    report = {
        "schema_version": 1,
        "dataset": args.dataset,
        "source_sha256": source_sha,
        "normalized_sha256": digest.hexdigest(),
        "source_edge_rows": source_rows,
        "output_directed_edge_rows": output_rows,
        "vertex_count_by_max_id": maximum + 1,
        "self_loop_rows": self_loops,
        "symmetrized": args.symmetrize,
        "drop_self_loops": args.drop_self_loops,
        "edge_order": "source order; reverse arc immediately follows each source arc when symmetrized",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
