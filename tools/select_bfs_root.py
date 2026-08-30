#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Select a deterministic BFS root from an ordered edge stream."
    )
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--imported-rate", type=float, required=True)
    parser.add_argument("--total-edges", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--sliding-window-aware",
        action="store_true",
        help=(
            "Choose the vertex maximizing the minimum out-degree across the initial "
            "imported prefix and the final equal-size sliding window. This avoids "
            "selecting a root that is high-degree initially but becomes isolated "
            "after the benchmark's source-order insert/delete stream."
        ),
    )
    args = parser.parse_args()

    if not (0.0 < args.imported_rate <= 1.0):
        raise ValueError("imported-rate must be in (0, 1]")
    imported_edges = int(args.total_edges * args.imported_rate)
    if imported_edges <= 0:
        raise ValueError("imported prefix is empty")
    if imported_edges > args.total_edges:
        raise ValueError("imported prefix exceeds total edge count")

    edges: list[tuple[int, int]] = []
    with args.input.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError("expected at least two edge columns")
            edges.append((int(parts[0]), int(parts[1])))

    if len(edges) != args.total_edges:
        raise ValueError(f"expected {args.total_edges} total edges, saw {len(edges)}")

    initial_degree: dict[int, int] = {}
    for u, _ in edges[:imported_edges]:
        initial_degree[u] = initial_degree.get(u, 0) + 1
    if not initial_degree:
        raise ValueError("no source vertices in imported prefix")

    if args.sliding_window_aware:
        updates = args.total_edges - imported_edges
        final_start = updates
        final_degree: dict[int, int] = {}
        for u, _ in edges[final_start:]:
            final_degree[u] = final_degree.get(u, 0) + 1

        candidates = set(initial_degree) & set(final_degree)
        if not candidates:
            raise ValueError("no source vertex appears in both initial and final windows")

        # Maximize the weaker endpoint degree first, then total endpoint degree,
        # then use the smallest dense ID for deterministic ties.
        root = min(
            candidates,
            key=lambda u: (
                -min(initial_degree[u], final_degree[u]),
                -(initial_degree[u] + final_degree[u]),
                u,
            ),
        )
        initial = initial_degree[root]
        final = final_degree[root]
        policy = (
            "maximum minimum out-degree across initial imported prefix and final "
            "equal-size sliding window; maximum summed endpoint degree secondary; "
            "smallest dense ID tie-break"
        )
    else:
        root, initial = min(initial_degree.items(), key=lambda item: (-item[1], item[0]))
        final = None
        final_start = None
        policy = "maximum out-degree in imported source-order prefix; smallest dense ID tie-break"

    report = {
        "schema_version": 2,
        "artifact_type": "velographx-bfs-root-selection",
        "research_claim": False,
        "policy": policy,
        "sliding_window_aware": args.sliding_window_aware,
        "imported_rate": args.imported_rate,
        "imported_edges": imported_edges,
        "root": root,
        "root_imported_out_degree": initial,
    }
    if args.sliding_window_aware:
        report.update(
            {
                "final_window_start_edge": final_start,
                "final_window_edges": imported_edges,
                "root_final_window_out_degree": final,
                "root_endpoint_min_out_degree": min(initial, final),
            }
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
