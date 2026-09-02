#!/usr/bin/env python3
"""Select one allowed logical CPU per physical core for publication runs."""

import argparse
import json
import os
import subprocess
from pathlib import Path


def topology():
    output = subprocess.check_output(["lscpu", "-p=CPU,CORE,SOCKET,ONLINE"], text=True)
    allowed = set(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else None
    cores = {}
    for row in output.splitlines():
        if row.startswith("#"):
            continue
        cpu, core, socket, online = row.split(",")
        cpu_number = int(cpu)
        if online.strip().upper() == "Y" and (allowed is None or cpu_number in allowed):
            cores.setdefault((int(socket), int(core)), []).append(cpu_number)
    if not cores:
        raise RuntimeError("no online CPUs are available to this process")
    return cores


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--threads", type=int, help="physical cores to use; default is all")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--shell-output", type=Path)
    args = parser.parse_args()
    cores = topology()
    representatives = [min(cpus) for _, cpus in sorted(cores.items())]
    requested = args.threads or len(representatives)
    if requested < 1 or requested > len(representatives):
        parser.error(f"threads must be between 1 and {len(representatives)}")
    selected = representatives[:requested]
    affinity = " ".join(map(str, selected))
    result = {
        "schema_version": 1, "artifact_type": "physical-core-affinity-plan",
        "available_physical_cores": len(representatives), "requested_threads": requested,
        "selected_cpu_list": selected, "gomp_cpu_affinity": affinity,
        "omp_environment": {"OMP_NUM_THREADS": str(requested), "OMP_DYNAMIC": "false",
                            "OMP_PROC_BIND": "true", "GOMP_CPU_AFFINITY": affinity},
        "selection": "lowest allowed online logical CPU for each unique (socket,core)",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    if args.shell_output:
        args.shell_output.write_text("\n".join(f"export {k}='{v}'" for k, v in result["omp_environment"].items()) + "\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
