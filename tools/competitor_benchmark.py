#!/usr/bin/env python3
import argparse
import hashlib
import importlib
import json
import os
import platform
import shlex
import subprocess
import sys
import time
from collections import deque
from pathlib import Path

SCHEMA_VERSION = 1
SUPPORTED = ("builtin", "networkx", "igraph", "networkit", "rustworkx", "external")


def read_edges(path: Path):
    edges = []
    max_vertex = -1
    with path.open("r", encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError(f"{path}:{lineno}: expected at least two integer columns")
            u, v = int(parts[0]), int(parts[1])
            if u < 0 or v < 0:
                raise ValueError(f"{path}:{lineno}: negative vertex id")
            edges.append((u, v))
            max_vertex = max(max_vertex, u, v)
    return edges, max_vertex + 1


def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def digest_result(result):
    payload = json.dumps(result, sort_keys=True, separators=(",", ":"), default=str).encode()
    return hashlib.sha256(payload).hexdigest()


def bfs_builtin(edges, vertices, source, directed):
    adj = [[] for _ in range(vertices)]
    for u, v in edges:
        adj[u].append(v)
        if not directed:
            adj[v].append(u)
    dist = [-1] * vertices
    if source >= vertices:
        return dist
    dist[source] = 0
    q = deque([source])
    while q:
        u = q.popleft()
        for v in adj[u]:
            if dist[v] < 0:
                dist[v] = dist[u] + 1
                q.append(v)
    return dist


def run_networkx(edges, vertices, source, directed):
    nx = importlib.import_module("networkx")
    graph = nx.DiGraph() if directed else nx.Graph()
    graph.add_nodes_from(range(vertices))
    graph.add_edges_from(edges)
    lengths = nx.single_source_shortest_path_length(graph, source)
    return [lengths.get(v, -1) for v in range(vertices)]


def run_igraph(edges, vertices, source, directed):
    ig = importlib.import_module("igraph")
    graph = ig.Graph(n=vertices, edges=edges, directed=directed)
    values = graph.distances(source=[source])[0]
    return [-1 if value == float("inf") else int(value) for value in values]


def run_networkit(edges, vertices, source, directed):
    nk = importlib.import_module("networkit")
    graph = nk.graph.Graph(vertices, weighted=False, directed=directed)
    for u, v in edges:
        graph.addEdge(u, v)
    bfs = nk.distance.BFS(graph, source, storePaths=False, storeNodesSortedByDistance=False)
    bfs.run()
    return [-1 if bfs.distance(v) >= 1e300 else int(bfs.distance(v)) for v in range(vertices)]


def run_rustworkx(edges, vertices, source, directed):
    rx = importlib.import_module("rustworkx")
    graph = rx.PyDiGraph() if directed else rx.PyGraph()
    graph.add_nodes_from([None] * vertices)
    graph.add_edges_from([(u, v, None) for u, v in edges])
    lengths = rx.dijkstra_shortest_path_lengths(graph, source, lambda _: 1.0)
    return [int(lengths[v]) if v in lengths else -1 for v in range(vertices)]


def validate_distances(values, vertices):
    if not isinstance(values, list) or len(values) != vertices:
        raise ValueError(f"competitor result must contain exactly {vertices} distances")
    result = []
    for value in values:
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise ValueError("competitor distances must be numeric")
        ivalue = int(value)
        if ivalue < -1 or ivalue != value:
            raise ValueError("competitor distances must be integers >= -1")
        result.append(ivalue)
    return result


def run_external(command, dataset, vertices, source, directed, timeout_seconds):
    if not command:
        raise ValueError("--external-command is required when --framework external is selected")
    argv = shlex.split(command)
    if not argv:
        raise ValueError("--external-command is empty")
    env = os.environ.copy()
    env.update(
        {
            "VELOGRAPHX_DATASET": str(dataset.resolve()),
            "VELOGRAPHX_ALGORITHM": "bfs",
            "VELOGRAPHX_SOURCE": str(source),
            "VELOGRAPHX_DIRECTED": "1" if directed else "0",
            "VELOGRAPHX_VERTICES": str(vertices),
        }
    )
    proc = subprocess.run(
        argv,
        check=False,
        capture_output=True,
        text=True,
        env=env,
        timeout=timeout_seconds,
    )
    if proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip() or f"exit code {proc.returncode}"
        raise RuntimeError(f"external competitor failed: {detail}")
    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        raise ValueError("external competitor stdout must be one JSON object") from exc
    if not isinstance(payload, dict) or "distances" not in payload:
        raise ValueError("external competitor JSON must contain a distances field")
    return validate_distances(payload["distances"], vertices), payload.get("framework_version")


def run_framework(name, edges, vertices, source, directed):
    if name == "builtin":
        return bfs_builtin(edges, vertices, source, directed)
    if name == "networkx":
        return run_networkx(edges, vertices, source, directed)
    if name == "igraph":
        return run_igraph(edges, vertices, source, directed)
    if name == "networkit":
        return run_networkit(edges, vertices, source, directed)
    if name == "rustworkx":
        return run_rustworkx(edges, vertices, source, directed)
    raise ValueError(f"unsupported in-process framework: {name}")


def package_version(name):
    if name == "builtin":
        return "python-stdlib"
    try:
        module = importlib.import_module(name)
        return str(getattr(module, "__version__", "unknown"))
    except Exception:
        return None


def main():
    parser = argparse.ArgumentParser(description="Run reproducible graph competitor benchmarks and emit machine-readable JSON.")
    parser.add_argument("--dataset", type=Path)
    parser.add_argument("--framework", choices=SUPPORTED, default="builtin")
    parser.add_argument("--algorithm", choices=("bfs",), default="bfs")
    parser.add_argument("--source", type=int, default=0)
    parser.add_argument("--directed", action="store_true")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--list-frameworks", action="store_true")
    parser.add_argument("--external-command", help="Command implementing the external JSON adapter contract")
    parser.add_argument("--external-name", default="external", help="Name recorded for an external native engine")
    parser.add_argument("--external-version", help="Version override when the external adapter does not report one")
    parser.add_argument("--timeout-seconds", type=float, default=300.0)
    args = parser.parse_args()

    if args.list_frameworks:
        print(json.dumps({"frameworks": list(SUPPORTED)}))
        return 0
    if args.dataset is None:
        raise ValueError("--dataset is required")
    if args.repeat < 1:
        raise ValueError("--repeat must be at least 1")
    if args.timeout_seconds <= 0:
        raise ValueError("--timeout-seconds must be positive")

    edges, vertices = read_edges(args.dataset)
    timings = []
    result = None
    external_reported_version = None
    for _ in range(args.repeat):
        start = time.perf_counter()
        if args.framework == "external":
            result, reported_version = run_external(
                args.external_command,
                args.dataset,
                vertices,
                args.source,
                args.directed,
                args.timeout_seconds,
            )
            if reported_version is not None:
                external_reported_version = str(reported_version)
        else:
            result = run_framework(args.framework, edges, vertices, args.source, args.directed)
        timings.append(time.perf_counter() - start)

    framework_name = args.external_name if args.framework == "external" else args.framework
    framework_version = (
        external_reported_version or args.external_version or "unknown"
        if args.framework == "external"
        else package_version(args.framework)
    )
    report = {
        "schema_version": SCHEMA_VERSION,
        "framework": framework_name,
        "adapter": args.framework,
        "framework_version": framework_version,
        "algorithm": args.algorithm,
        "dataset": str(args.dataset),
        "dataset_sha256": sha256_file(args.dataset),
        "directed": args.directed,
        "source": args.source,
        "vertices": vertices,
        "edges": len(edges),
        "repeat": args.repeat,
        "timings_seconds": timings,
        "median_seconds": sorted(timings)[len(timings) // 2],
        "result_digest": digest_result(result),
        "reachable_vertices": sum(1 for value in result if value >= 0),
        "environment": {
            "python": platform.python_version(),
            "platform": platform.platform(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "cpu_count": os.cpu_count(),
        },
    }
    text = json.dumps(report, sort_keys=True, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ModuleNotFoundError as exc:
        print(f"error: optional framework dependency is not installed: {exc.name}", file=sys.stderr)
        raise SystemExit(3)
    except subprocess.TimeoutExpired:
        print("error: external competitor timed out", file=sys.stderr)
        raise SystemExit(4)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
