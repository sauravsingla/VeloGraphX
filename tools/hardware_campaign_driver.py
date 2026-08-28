#!/usr/bin/env python3
import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import time


def build_command(args):
    command = list(args.command)
    wrappers = []

    if args.threads:
        if args.threads < 1:
            raise ValueError("--threads must be at least 1")
        taskset = shutil.which("taskset")
        if taskset:
            wrappers += [taskset, "-c", f"0-{args.threads - 1}"]

    if args.numa_policy != "none":
        numactl = shutil.which("numactl")
        if not numactl:
            raise RuntimeError("numactl is required for NUMA campaign modes")
        if args.numa_policy == "local":
            wrappers += [numactl, f"--cpunodebind={args.numa_node}", f"--membind={args.numa_node}"]
        elif args.numa_policy == "interleave":
            wrappers += [numactl, "--interleave=all"]
        elif args.numa_policy == "cross-node":
            if args.numa_cpu_node == args.numa_mem_node:
                raise ValueError("cross-node policy requires different CPU and memory nodes")
            wrappers += [numactl, f"--cpunodebind={args.numa_cpu_node}", f"--membind={args.numa_mem_node}"]

    if args.perf:
        perf = shutil.which("perf")
        if not perf:
            raise RuntimeError("perf is required when --perf is requested")
        events = args.perf_events or "cycles,instructions,cache-misses,branches,branch-misses"
        wrappers += [perf, "stat", "-x,", "-e", events, "--"]

    return wrappers + command


def main():
    parser = argparse.ArgumentParser(description="Run one VeloGraphX hardware-sensitive benchmark case reproducibly.")
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--numa-policy", choices=("none", "local", "interleave", "cross-node"), default="none")
    parser.add_argument("--numa-node", type=int, default=0)
    parser.add_argument("--numa-cpu-node", type=int, default=0)
    parser.add_argument("--numa-mem-node", type=int, default=1)
    parser.add_argument("--perf", action="store_true")
    parser.add_argument("--perf-events")
    parser.add_argument("--timeout-seconds", type=float, default=600)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    if not args.command:
        raise ValueError("benchmark command is required after options")
    if args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        raise ValueError("benchmark command is empty")

    command = build_command(args)
    env = os.environ.copy()
    env["VELOGRAPHX_THREADS"] = str(args.threads)
    env["OMP_NUM_THREADS"] = str(args.threads)

    started = time.perf_counter()
    completed = subprocess.run(command, env=env, text=True, capture_output=True, timeout=args.timeout_seconds, check=False)
    elapsed = time.perf_counter() - started

    report = {
        "schema_version": 1,
        "threads": args.threads,
        "numa_policy": args.numa_policy,
        "numa_node": args.numa_node if args.numa_policy == "local" else None,
        "numa_cpu_node": args.numa_cpu_node if args.numa_policy == "cross-node" else None,
        "numa_mem_node": args.numa_mem_node if args.numa_policy == "cross-node" else None,
        "perf_enabled": args.perf,
        "perf_events": args.perf_events,
        "argv": command,
        "returncode": completed.returncode,
        "wall_seconds": elapsed,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
        "environment": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "cpu_count": os.cpu_count(),
            "taskset_available": shutil.which("taskset") is not None,
            "numactl_available": shutil.which("numactl") is not None,
            "perf_available": shutil.which("perf") is not None,
        },
    }
    print(json.dumps(report, sort_keys=True))
    return completed.returncode


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.TimeoutExpired as exc:
        print(f"error: benchmark timed out: {exc}", file=sys.stderr)
        raise SystemExit(3)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
