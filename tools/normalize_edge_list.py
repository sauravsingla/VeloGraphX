#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description="Normalize an integer edge list to contiguous vertex IDs with deterministic deduplication.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--directed", action="store_true")
    args = parser.parse_args()

    raw_edges = []
    vertices = set()
    raw_rows = 0
    self_loops = 0
    with args.input.open("r", encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError(f"{args.input}:{lineno}: expected at least two columns")
            u, v = int(parts[0]), int(parts[1])
            if u < 0 or v < 0:
                raise ValueError(f"{args.input}:{lineno}: negative vertex id")
            raw_rows += 1
            vertices.update((u, v))
            if u == v:
                self_loops += 1
                continue
            raw_edges.append((u, v))

    ordered_vertices = sorted(vertices)
    remap = {vertex: index for index, vertex in enumerate(ordered_vertices)}
    normalized = set()
    for u, v in raw_edges:
        a, b = remap[u], remap[v]
        if not args.directed and a > b:
            a, b = b, a
        normalized.add((a, b))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as out:
        out.write("# VeloGraphX deterministic normalized edge list\n")
        out.write(f"# vertices {len(ordered_vertices)} edges {len(normalized)} directed {str(args.directed).lower()}\n")
        for u, v in sorted(normalized):
            out.write(f"{u}\t{v}\n")

    report = {
        "schema_version": 1,
        "artifact_type": "velographx-normalized-edge-list",
        "research_claim": False,
        "source_path": str(args.input),
        "source_sha256": sha256_file(args.input),
        "output_path": str(args.output),
        "output_sha256": sha256_file(args.output),
        "directed": args.directed,
        "raw_edge_rows": raw_rows,
        "self_loop_rows_removed": self_loops,
        "duplicate_or_reverse_rows_removed": raw_rows - self_loops - len(normalized),
        "vertex_count": len(ordered_vertices),
        "edge_count": len(normalized),
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
