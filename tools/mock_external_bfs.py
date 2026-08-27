#!/usr/bin/env python3
import json
import os
from collections import deque
from pathlib import Path


def main():
    dataset = Path(os.environ["VELOGRAPHX_DATASET"])
    source = int(os.environ["VELOGRAPHX_SOURCE"])
    directed = os.environ.get("VELOGRAPHX_DIRECTED", "0") == "1"
    vertices = int(os.environ["VELOGRAPHX_VERTICES"])

    adj = [[] for _ in range(vertices)]
    with dataset.open("r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            u, v = map(int, line.split()[:2])
            adj[u].append(v)
            if not directed:
                adj[v].append(u)

    dist = [-1] * vertices
    if 0 <= source < vertices:
        dist[source] = 0
        queue = deque([source])
        while queue:
            u = queue.popleft()
            for v in adj[u]:
                if dist[v] == -1:
                    dist[v] = dist[u] + 1
                    queue.append(v)

    print(json.dumps({"framework_version": "contract-fixture-1", "distances": dist}))


if __name__ == "__main__":
    main()
