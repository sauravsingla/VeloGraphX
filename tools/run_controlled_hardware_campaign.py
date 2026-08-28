#!/usr/bin/env python3
import argparse
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

DEFAULT_THREADS = [1, 2, 4, 8, 16, 32]
DEFAULT_PERF_EVENTS = "cycles,instructions,branches,branch-misses,cache-references,cache-misses"


def run(cmd, timeout):
    completed = subprocess.run(cmd, text=True, capture_output=True, timeout=timeout, check=False)
    return {"argv": cmd, "returncode": completed.returncode, "stdout": completed.stdout.strip(), "stderr": completed.stderr.strip()}


def detect_numa_nodes():
    if not shutil.which("numactl"):
        return []
    probe = subprocess.run(["numactl", "--hardware"], text=True, capture_output=True, check=False)
    if probe.returncode != 0:
        return []
    for line in probe.stdout.splitlines():
        if line.startswith("available:") and "nodes" in line:
            try:
                return list(range(int(line.split()[1])))
            except (ValueError, IndexError):
                return []
    return []


def parse_driver_report(result):
    if result["returncode"] != 0:
        return None
    lines = [line for line in result["stdout"].splitlines() if line.strip()]
    if not lines:
        return None
    try:
        return json.loads(lines[-1])
    except json.JSONDecodeError:
        return None


def summarize(samples):
    values = [sample["wall_seconds"] for sample in samples if sample.get("returncode") == 0 and sample.get("wall_seconds") is not None]
    if not values:
        return {"n": 0}
    ordered = sorted(values)
    p95_index = min(len(ordered) - 1, max(0, int(round(0.95 * (len(ordered) - 1)))))
    return {"n": len(values), "median_wall_seconds": statistics.median(values), "p95_wall_seconds": ordered[p95_index], "min_wall_seconds": min(values), "max_wall_seconds": max(values)}


def main():
    parser = argparse.ArgumentParser(description="Execute VeloGraphX controlled-hardware campaign cases.")
    parser.add_argument("--output", default="controlled-hardware-results.json")
    parser.add_argument("--repeats", type=int, default=10)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--threads", default=",".join(str(x) for x in DEFAULT_THREADS))
    parser.add_argument("--perf", action="store_true")
    parser.add_argument("--perf-events", default=DEFAULT_PERF_EVENTS)
    parser.add_argument("--numa", action="store_true")
    parser.add_argument("--timeout-seconds", type=float, default=600)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    if args.repeats < 1 or args.warmups < 0:
        raise ValueError("repeats must be >=1 and warmups must be >=0")
    command = list(args.command)
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        raise ValueError("benchmark command is required after --")

    threads = sorted(set(int(x) for x in args.threads.split(",") if x.strip()))
    if any(t < 1 for t in threads):
        raise ValueError("thread counts must be positive")
    cpu_count = os.cpu_count() or 1
    executable_threads = [t for t in threads if t <= cpu_count]
    skipped_threads = [t for t in threads if t > cpu_count]
    numa_nodes = detect_numa_nodes()
    driver = str(Path(__file__).with_name("hardware_campaign_driver.py"))
    cases = []

    def execute_case(kind, thread_count, numa_policy="none", numa_node=0, numa_cpu_node=0, numa_mem_node=1, perf=False):
        base = [sys.executable, driver, "--threads", str(thread_count), "--numa-policy", numa_policy]
        if numa_policy == "local":
            base += ["--numa-node", str(numa_node)]
        elif numa_policy == "cross-node":
            base += ["--numa-cpu-node", str(numa_cpu_node), "--numa-mem-node", str(numa_mem_node)]
        if perf:
            base += ["--perf", "--perf-events", args.perf_events]
        base += ["--timeout-seconds", str(args.timeout_seconds), "--"] + command

        warmup_failed = False
        for _ in range(args.warmups):
            warm = run(base, args.timeout_seconds + 30)
            if warm["returncode"] != 0:
                warmup_failed = True
                break

        samples = []
        for _ in range(args.repeats):
            result = run(base, args.timeout_seconds + 30)
            report = parse_driver_report(result)
            samples.append(report or {"returncode": result["returncode"], "wall_seconds": None, "stdout": result["stdout"], "stderr": result["stderr"]})

        cases.append({
            "kind": kind,
            "threads": thread_count,
            "numa_policy": numa_policy,
            "numa_node": numa_node if numa_policy == "local" else None,
            "numa_cpu_node": numa_cpu_node if numa_policy == "cross-node" else None,
            "numa_mem_node": numa_mem_node if numa_policy == "cross-node" else None,
            "perf": perf,
            "warmup_failed": warmup_failed,
            "summary": summarize(samples),
            "samples": samples,
        })

    for t in executable_threads:
        execute_case("thread_scaling", t)

    if args.perf:
        execute_case("hardware_counters", max(executable_threads) if executable_threads else 1, perf=True)

    if args.numa:
        if len(numa_nodes) >= 2:
            peak_threads = max(executable_threads) if executable_threads else 1
            execute_case("numa_local", peak_threads, numa_policy="local", numa_node=numa_nodes[0])
            execute_case("numa_local", peak_threads, numa_policy="local", numa_node=numa_nodes[1])
            execute_case("numa_interleave", peak_threads, numa_policy="interleave")
            execute_case("numa_cross_node", peak_threads, numa_policy="cross-node", numa_cpu_node=numa_nodes[0], numa_mem_node=numa_nodes[1])
        else:
            cases.append({"kind": "numa", "skipped": True, "reason": "fewer than two genuine NUMA nodes detected"})

    all_measured = [case for case in cases if not case.get("skipped")]
    all_success = bool(all_measured) and all(case.get("summary", {}).get("n", 0) == args.repeats and not case.get("warmup_failed", False) for case in all_measured)
    required_threads_present = all(t in executable_threads for t in DEFAULT_THREADS)
    numa_ready = (not args.numa) or len(numa_nodes) >= 2
    perf_ready = (not args.perf) or shutil.which("perf") is not None

    hardware_measurement_ready = bool(all_success and required_threads_present and numa_ready and perf_ready and args.repeats >= 10 and args.warmups >= 2)

    report = {
        "schema_version": 1,
        "artifact_type": "velographx-controlled-hardware-campaign",
        "research_claim": False,
        "publication_ready": False,
        "hardware_measurement_ready": hardware_measurement_ready,
        "claim_gate": {
            "all_measurements_succeeded": all_success,
            "threads_1_2_4_8_16_32_available": required_threads_present,
            "genuine_numa_requirement_met": numa_ready,
            "perf_requirement_met": perf_ready,
            "minimum_repeats_met": args.repeats >= 10,
            "minimum_warmups_met": args.warmups >= 2,
            "competitor_and_dataset_validation_required_separately": True,
            "allow_publication_claims": False,
            "note": "This artifact can establish hardware-measurement readiness only. Publication readiness requires pinned datasets, exact correctness, same-hardware native competitors, ablations, environment capture, and final repository validation."
        },
        "host": {
            "platform": platform.platform(), "machine": platform.machine(), "cpu_count": cpu_count, "numa_nodes": numa_nodes,
            "taskset_available": shutil.which("taskset") is not None, "numactl_available": shutil.which("numactl") is not None, "perf_available": shutil.which("perf") is not None,
        },
        "configuration": {
            "requested_threads": threads, "executed_threads": executable_threads, "skipped_threads": skipped_threads,
            "repeats": args.repeats, "warmups": args.warmups, "perf": args.perf,
            "perf_events": args.perf_events if args.perf else None, "numa": args.numa, "command": command,
        },
        "cases": cases,
    }

    Path(args.output).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"output": args.output, "hardware_measurement_ready": hardware_measurement_ready, "publication_ready": False, "research_claim": False, "executed_threads": executable_threads, "skipped_threads": skipped_threads, "numa_nodes": numa_nodes, "cases": len(cases)}, sort_keys=True))
    return 0 if all_success else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
