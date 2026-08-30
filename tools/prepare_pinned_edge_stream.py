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


def parse_edge(path: Path, lineno: int, raw: str):
    line = raw.strip()
    if not line or line.startswith("#"):
        return None
    parts = line.split()
    if len(parts) < 2:
        raise ValueError(f"{path}:{lineno}: expected at least two columns")
    u, v = int(parts[0]), int(parts[1])
    if u < 0 or v < 0:
        raise ValueError(f"{path}:{lineno}: negative vertex id")
    if u == v:
        raise ValueError(f"{path}:{lineno}: unexpected self-loop in pinned source")
    return u, v


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare a checksum-pinned integer edge stream without reordering edges. "
            "Sparse source vertex IDs are deterministically relabeled to a dense range."
        )
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

    vertices = set()
    edge_count = 0
    raw_min_vertex = None
    raw_max_vertex = None
    with args.input.open("r", encoding="utf-8") as src:
        for lineno, raw in enumerate(src, 1):
            edge = parse_edge(args.input, lineno, raw)
            if edge is None:
                continue
            u, v = edge
            vertices.add(u)
            vertices.add(v)
            edge_count += 1
            raw_min_vertex = min(u, v) if raw_min_vertex is None else min(raw_min_vertex, u, v)
            raw_max_vertex = max(u, v) if raw_max_vertex is None else max(raw_max_vertex, u, v)

    vertex_count = len(vertices)
    if edge_count != args.expected_edges:
        raise ValueError(f"edge count mismatch: expected {args.expected_edges}, got {edge_count}")
    if vertex_count != args.expected_vertices:
        raise ValueError(f"vertex count mismatch: expected {args.expected_vertices}, got {vertex_count}")

    ordered_vertices = sorted(vertices)
    remap = {vertex: dense for dense, vertex in enumerate(ordered_vertices)}

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.input.open("r", encoding="utf-8") as src, args.output.open("w", encoding="utf-8") as out:
        for lineno, raw in enumerate(src, 1):
            edge = parse_edge(args.input, lineno, raw)
            if edge is None:
                continue
            u, v = edge
            out.write(f"{remap[u]}\t{remap[v]}\n")

    report = {
        "schema_version": 2,
        "artifact_type": "velographx-pinned-edge-stream",
        "research_claim": False,
        "source_path": str(args.input),
        "source_sha256": actual_sha,
        "output_path": str(args.output),
        "output_sha256": sha256_file(args.output),
        "preparation": "checksum-pinned source order; comments stripped; no edge reordering",
        "vertex_count": vertex_count,
        "edge_count": edge_count,
        "self_loop_rows": 0,
        "raw_min_vertex": raw_min_vertex,
        "raw_max_vertex": raw_max_vertex,
        "dense_min_vertex": 0 if vertex_count else None,
        "dense_max_vertex": vertex_count - 1 if vertex_count else None,
        "vertex_relabeling": "ascending original vertex ID -> contiguous 0..N-1",
        "source_vertex_zero_present": 0 in vertices,
        "source_vertex_zero_dense_id": remap.get(0),
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
