#!/usr/bin/env python3
"""Run the publication-grade in-memory capacity boundary campaign.

The sweep generates deterministic Kronecker workloads, executes the public
benchmark under a per-process POSIX resource wrapper, records peak RSS and Linux swap activity,
and stops at the first scale that cannot complete as a clean in-memory run.

A capacity boundary is established only when at least one scale succeeds and a
larger attempted scale fails, swaps, or is otherwise rejected. If every
configured scale succeeds, extend the scale list and run again.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def read_meminfo() -> dict[str, int]:
    values: dict[str, int] = {}
    path = Path("/proc/meminfo")
    if not path.exists():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        key, rest = line.split(":", 1)
        match = re.search(r"(\d+)", rest)
        if match:
            values[key] = int(match.group(1)) * 1024
    return values


def read_vmstat() -> dict[str, int]:
    values: dict[str, int] = {}
    path = Path("/proc/vmstat")
    if not path.exists():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        key, value = line.split()[:2]
        if key in {"pswpin", "pswpout", "oom_kill"}:
            values[key] = int(value)
    return values


def capture_command(command: list[str]) -> str:
    try:
        return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()
    except Exception as exc:  # noqa: BLE001 - environment capture should not abort campaign
        return f"unavailable: {exc}"


def run_checked(command: list[str], **kwargs) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, check=True, **kwargs)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scales", default="16,18,20,22,24,26")
    parser.add_argument("--edgefactor", type=int, default=16)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--data-dir", type=Path, default=Path("capacity-data"))
    parser.add_argument("--output-dir", type=Path, default=Path("capacity-artifacts"))
    parser.add_argument("--benchmark-binary", type=Path)
    parser.add_argument("--source", type=int, default=0)
    parser.add_argument("--keep-edge-files", action="store_true")
    parser.add_argument("--require-boundary", action="store_true")
    args = parser.parse_args()

    scales = [int(item.strip()) for item in args.scales.split(",") if item.strip()]
    if not scales or scales != sorted(set(scales)):
        raise SystemExit("--scales must be a non-empty, strictly increasing unique list")

    root = Path(__file__).resolve().parents[1]
    prepare_script = root / "scripts" / "prepare_publication_datasets.py"
    resource_helper = root / "tools" / "run_resource_measurement.py"
    benchmark = args.benchmark_binary or (args.build_dir / "velographx_public_dataset_benchmark")
    benchmark = benchmark.resolve()
    args.data_dir.mkdir(parents=True, exist_ok=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    if not benchmark.exists():
        raise SystemExit(f"benchmark binary not found: {benchmark}")
    if not resource_helper.is_file():
        raise SystemExit(f"resource measurement helper not found: {resource_helper}")

    environment = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "hostname": platform.node(),
        "platform": platform.platform(),
        "python": sys.version,
        "cpu": capture_command(["lscpu"]),
        "numactl_hardware": capture_command(["numactl", "--hardware"]) if shutil.which("numactl") else "numactl unavailable",
        "git_head": capture_command(["git", "rev-parse", "HEAD"]),
        "git_status": capture_command(["git", "status", "--porcelain"]),
        "meminfo_start": read_meminfo(),
        "vmstat_start": read_vmstat(),
        "thread_environment": {k: os.environ.get(k) for k in ["OMP_NUM_THREADS", "OMP_PROC_BIND", "OMP_PLACES"]},
        "contract": {
            "generator": "Graph500-style deterministic R-MAT",
            "A": 0.57,
            "B": 0.19,
            "C": 0.19,
            "D": 0.05,
            "edgefactor": args.edgefactor,
            "seed": args.seed,
            "scales": scales,
            "accept_in_memory_only_if": "benchmark exit=0 and pswpin/pswpout deltas are both zero",
        },
    }
    (args.output_dir / "environment.json").write_text(json.dumps(environment, indent=2) + "\n", encoding="utf-8")

    records: list[dict[str, object]] = []
    largest_success: int | None = None
    first_rejected: int | None = None

    for scale in scales:
        prefix = f"kron-scale{scale}-ef{args.edgefactor}-seed{args.seed}"
        edge_path = args.data_dir / f"{prefix}.edges"
        metadata_path = args.data_dir / f"{prefix}.metadata.json"
        prepare_log = args.output_dir / f"{prefix}.prepare.log"
        csv_path = args.output_dir / f"{prefix}.csv"
        stderr_path = args.output_dir / f"{prefix}.stderr.txt"
        resource_path = args.output_dir / f"{prefix}.resources.json"

        prepare_cmd = [
            sys.executable,
            str(prepare_script),
            "kronecker",
            "--output-dir",
            str(args.data_dir),
            "--scale",
            str(scale),
            "--edgefactor",
            str(args.edgefactor),
            "--seed",
            str(args.seed),
        ]

        prepare_rc = 0
        try:
            with prepare_log.open("w", encoding="utf-8") as log:
                run_checked(prepare_cmd, stdout=log, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as exc:
            prepare_rc = exc.returncode

        record: dict[str, object] = {
            "scale": scale,
            "vertices_configured": 1 << scale,
            "edgefactor": args.edgefactor,
            "edge_tuples_configured": args.edgefactor * (1 << scale),
            "seed": args.seed,
            "prepare_exit_code": prepare_rc,
        }

        if prepare_rc != 0 or not edge_path.exists() or not metadata_path.exists():
            record.update({"status": "rejected", "reason": "generation_failed"})
            records.append(record)
            first_rejected = scale
            break

        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        record["dataset_sha256"] = metadata.get("sha256")
        vm_before = read_vmstat()
        mem_before = read_meminfo()

        command = [
            sys.executable,
            str(resource_helper),
            "--output",
            str(resource_path),
            "--",
            str(benchmark),
            str(edge_path),
            str(args.source),
        ]
        with csv_path.open("w", encoding="utf-8") as out, stderr_path.open("w", encoding="utf-8") as err:
            completed = subprocess.run(command, text=True, stdout=out, stderr=err)

        vm_after = read_vmstat()
        mem_after = read_meminfo()
        timing = json.loads(resource_path.read_text(encoding="utf-8")) if resource_path.exists() else {}
        swapin_delta = vm_after.get("pswpin", 0) - vm_before.get("pswpin", 0)
        swapout_delta = vm_after.get("pswpout", 0) - vm_before.get("pswpout", 0)
        oom_delta = vm_after.get("oom_kill", 0) - vm_before.get("oom_kill", 0)
        clean_in_memory = completed.returncode == 0 and swapin_delta == 0 and swapout_delta == 0 and oom_delta == 0

        record.update(
            {
                "benchmark_exit_code": completed.returncode,
                "peak_rss_bytes": timing.get("peak_rss_bytes"),
                "elapsed_wall_seconds": timing.get("wall_seconds"),
                "pswpin_pages_delta": swapin_delta,
                "pswpout_pages_delta": swapout_delta,
                "oom_kill_delta": oom_delta,
                "swap_total_bytes": mem_before.get("SwapTotal"),
                "swap_free_before_bytes": mem_before.get("SwapFree"),
                "swap_free_after_bytes": mem_after.get("SwapFree"),
                "status": "accepted-in-memory" if clean_in_memory else "rejected",
                "reason": None if clean_in_memory else (
                    "benchmark_failed" if completed.returncode != 0 else "swap_or_oom_activity_detected"
                ),
            }
        )
        records.append(record)

        if clean_in_memory:
            largest_success = scale
        else:
            first_rejected = scale

        if not args.keep_edge_files:
            edge_path.unlink(missing_ok=True)

        if not clean_in_memory:
            break

    boundary_established = largest_success is not None and first_rejected is not None and first_rejected > largest_success
    summary = {
        "schema_version": 1,
        "campaign": "largest-practical-in-memory",
        "largest_accepted_scale": largest_success,
        "first_rejected_scale": first_rejected,
        "boundary_established": boundary_established,
        "configured_scales": scales,
        "records": records,
        "interpretation": (
            "Capacity boundary established: largest accepted scale completed with zero observed swap/OOM activity and a larger attempted scale was rejected."
            if boundary_established
            else "Capacity boundary not yet established; extend the configured scale list or investigate why no scale completed."
        ),
    }
    summary_path = args.output_dir / "capacity-summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    csv_summary = args.output_dir / "capacity-summary.csv"
    fields = [
        "scale", "vertices_configured", "edge_tuples_configured", "peak_rss_bytes",
        "benchmark_exit_code", "pswpin_pages_delta", "pswpout_pages_delta", "oom_kill_delta",
        "status", "reason",
    ]
    with csv_summary.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(records)

    print(json.dumps(summary, indent=2))
    if args.require_boundary and not boundary_established:
        return 3
    return 0 if largest_success is not None else 2


if __name__ == "__main__":
    raise SystemExit(main())
