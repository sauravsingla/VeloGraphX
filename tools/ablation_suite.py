#!/usr/bin/env python3
import argparse
import csv
import io
import json
import platform
import subprocess
import sys
import time
from pathlib import Path

SUITES = {
    "intersection": "velographx_intersection_benchmark",
    "compression": "velographx_compression_benchmark",
    "incremental": "velographx_update_fraction_benchmark",
}


def normalize_csv_row(row):
    """Convert DictReader rows into JSON-safe string-keyed dictionaries.

    csv.DictReader stores surplus columns under a None key. Preserve those
    values explicitly so evidence is not lost and json.dumps(sort_keys=True)
    remains deterministic.
    """
    normalized = {}
    extras = row.get(None)
    for key, value in row.items():
        if key is None:
            continue
        normalized[str(key)] = value
    if extras:
        normalized["__extra_fields__"] = list(extras)
    return normalized


def run_csv(executable: Path):
    started = time.perf_counter()
    completed = subprocess.run([str(executable)], text=True, capture_output=True, check=False)
    elapsed = time.perf_counter() - started
    rows = []
    if completed.returncode == 0:
        rows = [normalize_csv_row(row) for row in csv.DictReader(io.StringIO(completed.stdout))]
    return {
        "executable": str(executable),
        "returncode": completed.returncode,
        "wall_seconds": elapsed,
        "rows": rows,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
    }


def main():
    parser = argparse.ArgumentParser(description="Run VeloGraphX paired ablation measurements and emit JSON.")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--suite", action="append", choices=tuple(SUITES), help="Subset to run; repeatable.")
    args = parser.parse_args()

    selected = args.suite or list(SUITES)
    report = {
        "schema_version": 1,
        "kind": "paired-ablation-suite",
        "note": "These are paired measurements exposed by existing benchmarks; hardware-sensitive NUMA/thread/perf campaigns remain separate.",
        "environment": {
            "python": platform.python_version(),
            "platform": platform.platform(),
            "machine": platform.machine(),
        },
        "suites": {},
    }

    failed = False
    for name in selected:
        executable = args.build_dir / SUITES[name]
        if not executable.is_file():
            report["suites"][name] = {
                "executable": str(executable),
                "returncode": None,
                "error": "benchmark executable not found",
            }
            failed = True
            continue
        result = run_csv(executable)
        report["suites"][name] = result
        failed = failed or result["returncode"] != 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    return 2 if failed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
