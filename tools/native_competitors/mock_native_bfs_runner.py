#!/usr/bin/env python3
"""Deterministic CLI fixture for testing native competitor adapter shims."""

import argparse
import json
from collections import deque
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", type=Path, required=True)
    parser.add_argument("--source", type=int, required=True)
    parser.add_argument("--vertices", type=int, required=True)
    parser.add_argument("--directed", action="store_true")
    args = parser.parse_args()

    adj = [[] for _ in range(args.vertices)]
    with args.dataset.open("r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            u, v = map(int, line.split()[:2])
            adj[u].append(v)
            if not args.directed:
                adj[v].append(u)

    distances = [-1] * args.vertices
    if 0 <= args.source < args.vertices:
        distances[args.source] = 0
        q = deque([args.source])
        while q:
            u = q.popleft()
            for v in adj[u]:
                if distances[v] < 0:
                    distances[v] = distances[u] + 1
                    q.append(v)

    print(json.dumps({"framework_version": "native-wrapper-fixture-1", "distances": distances}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
