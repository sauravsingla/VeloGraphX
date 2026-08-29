#!/usr/bin/env python3
import argparse
import csv
import io
import json
import os
import shutil
import statistics
import subprocess
import sys
from pathlib import Path


def run(command, *, timeout=600):
    return subprocess.run(command, text=True, capture_output=True, timeout=timeout, check=False)


def allocated_cpus():
    if hasattr(os, "sched_getaffinity"):
        try:
            cpus = sorted(os.sched_getaffinity(0))
            if cpus:
                return cpus
        except OSError:
            pass
    return list(range(max(1, os.cpu_count() or 1)))


def parse_lscpu():
    fields = {}
    result = run(["lscpu", "-J"], timeout=30) if shutil.which("lscpu") else None
    if result and result.returncode == 0:
        try:
            payload = json.loads(result.stdout)
            for row in payload.get("lscpu", []):
                key = str(row.get("field", "")).rstrip(":")
                fields[key] = str(row.get("data", ""))
        except json.JSONDecodeError:
            pass

    def number(name, fallback):
        try:
            return int(fields.get(name, fallback))
        except (TypeError, ValueError):
            return fallback

    cpus = allocated_cpus()
    host_logical = max(1, number("CPU(s)", os.cpu_count() or 1))
    return {
        "logical_cpus": len(cpus),
        "allocated_cpu_ids": cpus,
        "host_logical_cpus": host_logical,
        "sockets": max(1, number("Socket(s)", 1)),
        "cores_per_socket": max(1, number("Core(s) per socket", host_logical)),
        "threads_per_core": max(1, number("Thread(s) per core", 1)),
        "numa_nodes": max(1, number("NUMA node(s)", 1)),
        "model_name": fields.get("Model name", "unknown"),
        "architecture": fields.get("Architecture", "unknown"),
        "raw_lscpu": fields,
    }


def available_numa_nodes():
    if not shutil.which("numactl"):
        return []
    result = run(["numactl", "--hardware"], timeout=30)
    if result.returncode != 0:
        return []
    nodes = []
    for line in result.stdout.splitlines():
        if line.startswith("available:") and "nodes" in line:
            try:
                count = int(line.split()[1])
                nodes = list(range(count))
            except (ValueError, IndexError):
                pass
    return nodes


def parse_thread_list(text):
    values = sorted({int(item) for item in text.split(",") if item.strip()})
    if not values or values[0] < 1:
        raise ValueError("thread list must contain positive integers")
    if 1 not in values:
        values.insert(0, 1)
    return values


def parse_benchmark_csv(text):
    rows = list(csv.DictReader(io.StringIO(text.strip())))
    parsed = []
    for row in rows:
        parsed.append({
            "threads": int(row["threads"]),
            "queries": int(row["queries"]),
            "total_ns": int(row["total_ns"]),
            "queries_per_second": float(row["queries_per_second"]),
            "speedup": float(row["speedup"]),
            "parallel_efficiency": float(row["parallel_efficiency"]),
            "digest": int(row["digest"]),
        })
    if not parsed:
        raise RuntimeError("thread-scaling benchmark produced no rows")
    return parsed


def invoke(binary, edge_list, queries, threads, prefix=None, timeout=600):
    command = []
    if prefix:
        command.extend(prefix)
    command.extend([str(binary), str(edge_list), str(queries), ",".join(map(str, threads))])
    completed = run(command, timeout=timeout)
    if completed.returncode != 0:
        raise RuntimeError(
            f"benchmark failed ({completed.returncode}): {' '.join(command)}\n{completed.stderr}"
        )
    return parse_benchmark_csv(completed.stdout), command


def aggregate(repetitions):
    by_thread = {}
    for repetition in repetitions:
        for row in repetition:
            by_thread.setdefault(row["threads"], []).append(row)

    baseline_qps = statistics.median(
        row["queries_per_second"] for row in by_thread[1]
    )
    digest = by_thread[1][0]["digest"]
    output = []
    for threads in sorted(by_thread):
        rows = by_thread[threads]
        if any(row["digest"] != digest for row in rows):
            raise RuntimeError(f"BFS digest mismatch at {threads} threads")
        qps_samples = [row["queries_per_second"] for row in rows]
        median_qps = statistics.median(qps_samples)
        speedup = median_qps / baseline_qps if baseline_qps else 0.0
        output.append({
            "threads": threads,
            "queries": rows[0]["queries"],
            "repeats": len(rows),
            "median_queries_per_second": median_qps,
            "min_queries_per_second": min(qps_samples),
            "max_queries_per_second": max(qps_samples),
            "speedup": speedup,
            "parallel_efficiency": speedup / threads,
            "digest": digest,
        })
    return output


def run_repeated(binary, edge_list, queries, threads, repeats, prefix=None, timeout=600):
    repetitions = []
    command = None
    for _ in range(repeats):
        rows, command = invoke(binary, edge_list, queries, threads, prefix=prefix, timeout=timeout)
        repetitions.append(rows)
    return {
        "command": command,
        "repetitions": repetitions,
        "aggregate": aggregate(repetitions),
    }


def parse_cpu_list(text):
    cpus = set()
    for part in text.strip().split(","):
        if not part:
            continue
        if "-" in part:
            start, end = map(int, part.split("-", 1))
            cpus.update(range(start, end + 1))
        else:
            cpus.add(int(part))
    return cpus


def node_cpu_ids(node):
    path = Path(f"/sys/devices/system/node/node{node}/cpulist")
    if not path.exists():
        return set()
    try:
        return parse_cpu_list(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return set()


def node_allocated_cpu_count(node, allocation):
    return len(node_cpu_ids(node).intersection(allocation))


def markdown(report):
    topology = report["topology"]
    lines = [
        "# CPU Scaling Evidence",
        "",
        f"Detected allocation: **{topology['logical_cpus']} usable logical CPUs**; host topology reports **{topology['sockets']} socket(s)** and **{topology['numa_nodes']} NUMA node(s)**.",
        "",
        "The requested paper ladder is `1 / 2 / 4 / 8 / 16 / 32` threads. Points above the process CPU-affinity allocation are not oversubscribed; they are recorded as unavailable.",
        "",
        "| Threads | Median queries/s | Speedup | Parallel efficiency |",
        "|---:|---:|---:|---:|",
    ]
    for row in report["thread_scaling"]["aggregate"]:
        lines.append(
            f"| {row['threads']} | {row['median_queries_per_second']:.2f} | {row['speedup']:.3f}x | {100.0 * row['parallel_efficiency']:.1f}% |"
        )
    lines.extend([
        "",
        f"Available requested points: `{', '.join(map(str, report['feasibility']['available_threads']))}`.",
        f"Unavailable requested points: `{', '.join(map(str, report['feasibility']['unavailable_threads'])) or 'none'}`.",
        "",
    ])
    numa = report["numa"]
    if numa["feasible"]:
        lines.extend([
            "## NUMA comparison",
            "",
            "A dual-node comparison was executed because the current CPU-affinity allocation spans at least two NUMA nodes on a multi-socket host.",
            "",
            f"Single-node comparison threads: {numa['single_node_threads']}; dual-node comparison threads: {numa['dual_node_threads']}.",
            "",
        ])
    else:
        lines.extend([
            "## NUMA comparison",
            "",
            f"Not executable on this allocation: {numa['reason']}",
            "",
        ])
    lines.extend([
        "## Interpretation boundary",
        "",
        "Hosted-runner results are reproducible engineering evidence. A publication claim for 8/16/32+ cores or 1-socket vs 2-socket NUMA requires hardware whose active CPU allocation physically exposes those resources and should use the controlled self-hosted workflow.",
        "",
    ])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Run topology-aware VeloGraphX BFS CPU scaling evidence.")
    parser.add_argument("--binary", required=True)
    parser.add_argument("--edge-list", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--queries", type=int, default=64)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--threads", default="1,2,4,8,16,32")
    parser.add_argument("--timeout-seconds", type=int, default=600)
    parser.add_argument("--numa", action="store_true")
    args = parser.parse_args()

    if args.queries < 1 or args.repeats < 1:
        raise ValueError("queries and repeats must be positive")

    requested = parse_thread_list(args.threads)
    topology = parse_lscpu()
    allocation = set(topology["allocated_cpu_ids"])
    available = [threads for threads in requested if threads <= topology["logical_cpus"]]
    unavailable = [threads for threads in requested if threads > topology["logical_cpus"]]
    if 1 not in available:
        available.insert(0, 1)

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    scaling = run_repeated(
        args.binary,
        args.edge_list,
        args.queries,
        available,
        args.repeats,
        timeout=args.timeout_seconds,
    )

    numa_nodes = available_numa_nodes()
    allocated_nodes = [node for node in numa_nodes if node_allocated_cpu_count(node, allocation) > 0]
    numa_report = {
        "requested": bool(args.numa),
        "feasible": False,
        "reason": "NUMA comparison was not requested",
    }
    if args.numa:
        if not shutil.which("numactl"):
            numa_report["reason"] = "numactl is unavailable"
        elif topology["sockets"] < 2 or len(allocated_nodes) < 2:
            numa_report["reason"] = (
                f"host reports {topology['sockets']} socket(s), but the active CPU allocation spans "
                f"{len(allocated_nodes)} NUMA node(s); at least two allocated nodes on a multi-socket host "
                "are required for the paper-quality dual-socket comparison"
            )
        else:
            node0, node1 = allocated_nodes[:2]
            node0_cpus = node_allocated_cpu_count(node0, allocation)
            node1_cpus = node_allocated_cpu_count(node1, allocation)
            per_node = max(1, min(node0_cpus, node1_cpus, max(available)))
            single_threads = max(t for t in available if t <= per_node)
            dual_capacity = min(topology["logical_cpus"], node0_cpus + node1_cpus)
            dual_candidates = [t for t in available if t <= dual_capacity]
            dual_threads = max(dual_candidates)
            single = run_repeated(
                args.binary, args.edge_list, args.queries, [1, single_threads], args.repeats,
                prefix=["numactl", f"--cpunodebind={node0}", f"--membind={node0}"],
                timeout=args.timeout_seconds,
            )
            dual = run_repeated(
                args.binary, args.edge_list, args.queries, [1, dual_threads], args.repeats,
                prefix=["numactl", f"--cpunodebind={node0},{node1}", f"--interleave={node0},{node1}"],
                timeout=args.timeout_seconds,
            )
            same_threads = run_repeated(
                args.binary, args.edge_list, args.queries, [1, single_threads], args.repeats,
                prefix=["numactl", f"--cpunodebind={node0},{node1}", f"--interleave={node0},{node1}"],
                timeout=args.timeout_seconds,
            )
            numa_report = {
                "requested": True,
                "feasible": True,
                "nodes": [node0, node1],
                "node_cpu_counts": {str(node0): node0_cpus, str(node1): node1_cpus},
                "single_node_threads": single_threads,
                "dual_node_threads": dual_threads,
                "single_node_local": single,
                "two_node_same_threads": same_threads,
                "two_node_full_threads": dual,
            }

    report = {
        "schema_version": 2,
        "artifact_type": "velographx-cpu-scaling-evidence",
        "research_claim": False,
        "topology": topology,
        "requested_threads": requested,
        "feasibility": {
            "available_threads": available,
            "unavailable_threads": unavailable,
            "full_1_2_4_8_16_32_ladder": all(t in available for t in [1, 2, 4, 8, 16, 32]),
            "dual_socket_numa": bool(numa_report.get("feasible")),
        },
        "thread_scaling": scaling,
        "numa": numa_report,
    }

    (output_dir / "cpu-scaling.json").write_text(
        json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8"
    )
    (output_dir / "README.md").write_text(markdown(report), encoding="utf-8")

    with (output_dir / "thread-scaling.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["threads", "queries", "repeats", "median_queries_per_second", "min_queries_per_second", "max_queries_per_second", "speedup", "parallel_efficiency", "digest"],
        )
        writer.writeheader()
        writer.writerows(scaling["aggregate"])

    print(json.dumps({
        "logical_cpus": topology["logical_cpus"],
        "host_logical_cpus": topology["host_logical_cpus"],
        "sockets": topology["sockets"],
        "numa_nodes": topology["numa_nodes"],
        "allocated_numa_nodes": allocated_nodes,
        "available_threads": available,
        "unavailable_threads": unavailable,
        "dual_socket_numa": numa_report.get("feasible", False),
    }, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
