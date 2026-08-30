#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Select a deterministic BFS root from the imported prefix of an ordered edge stream."
    )
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--imported-rate", type=float, required=True)
    parser.add_argument("--total-edges", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    if not (0.0 < args.imported_rate <= 1.0):
        raise ValueError("imported-rate must be in (0, 1]")
    imported_edges = int(args.total_edges * args.imported_rate)
    if imported_edges <= 0:
        raise ValueError("imported prefix is empty")

    out_degree = {}
    rows = 0
    with args.input.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if rows >= imported_edges:
                break
            parts = line.split()
            if len(parts) < 2:
                raise ValueError("expected at least two edge columns")
            u = int(parts[0])
            out_degree[u] = out_degree.get(u, 0) + 1
            rows += 1

    if rows != imported_edges:
        raise ValueError(f"expected {imported_edges} imported edges, saw {rows}")
    if not out_degree:
        raise ValueError("no source vertices in imported prefix")

    # Highest imported-prefix out-degree, then smallest dense ID for stable ties.
    root, degree = min(out_degree.items(), key=lambda item: (-item[1], item[0]))
    report = {
        "schema_version": 1,
        "artifact_type": "velographx-bfs-root-selection",
        "research_claim": False,
        "policy": "maximum out-degree in imported source-order prefix; smallest dense ID tie-break",
        "imported_rate": args.imported_rate,
        "imported_edges": imported_edges,
        "root": root,
        "root_imported_out_degree": degree,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
