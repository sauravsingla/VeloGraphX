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
    parser = argparse.ArgumentParser(
        description="Stream a checksum-pinned integer edge list without global edge sorting."
    )
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--expected-sha256", required=True)
    parser.add_argument("--expected-vertices", type=int, required=True)
    parser.add_argument("--expected-edges", type=int, required=True)
    args = parser.parse_args()

    expected_sha = args.expected_sha256.lower()
    actual_sha = sha256_file(args.input)
    if actual_sha != expected_sha:
        raise ValueError(f"source sha256 mismatch: expected {expected_sha}, got {actual_sha}")

    seen = bytearray(args.expected_vertices)
    edge_count = 0
    self_loops = 0
    min_vertex = args.expected_vertices
    max_vertex = -1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.input.open("r", encoding="utf-8") as src, args.output.open("w", encoding="utf-8") as out:
        for lineno, raw in enumerate(src, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError(f"{args.input}:{lineno}: expected at least two columns")
            u, v = int(parts[0]), int(parts[1])
            if u < 0 or v < 0 or u >= args.expected_vertices or v >= args.expected_vertices:
                raise ValueError(f"{args.input}:{lineno}: vertex outside expected range")
            if u == v:
                self_loops += 1
                raise ValueError(f"{args.input}:{lineno}: unexpected self-loop in pinned source")
            seen[u] = 1
            seen[v] = 1
            min_vertex = min(min_vertex, u, v)
            max_vertex = max(max_vertex, u, v)
            edge_count += 1
            out.write(f"{u}\t{v}\n")

    vertex_count = sum(seen)
    if edge_count != args.expected_edges:
        raise ValueError(f"edge count mismatch: expected {args.expected_edges}, got {edge_count}")
    if vertex_count != args.expected_vertices:
        raise ValueError(f"vertex count mismatch: expected {args.expected_vertices}, got {vertex_count}")
    if min_vertex != 0 or max_vertex != args.expected_vertices - 1:
        raise ValueError(
            f"vertex range mismatch: expected 0..{args.expected_vertices - 1}, got {min_vertex}..{max_vertex}"
        )

    report = {
        "schema_version": 1,
        "artifact_type": "velographx-pinned-edge-stream",
        "research_claim": False,
        "source_path": str(args.input),
        "source_sha256": actual_sha,
        "output_path": str(args.output),
        "output_sha256": sha256_file(args.output),
        "preparation": "checksum-pinned source order; comments stripped; no edge reordering",
        "vertex_count": vertex_count,
        "edge_count": edge_count,
        "self_loop_rows": self_loops,
        "min_vertex": min_vertex,
        "max_vertex": max_vertex,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
