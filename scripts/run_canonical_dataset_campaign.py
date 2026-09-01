#!/usr/bin/env python3
"""Run repeated end-to-end measurements over the canonical real datasets.

The input manifest pins every source archive/file by SHA-256 and declares its
graph family and expected source statistics.  Each measured execution starts
from the normalized edge list, runs under ``/usr/bin/time -v``, and preserves
the raw CSV, stderr, and peak-RSS report.  The resulting summary is deliberately
not a publication claim; the final campaign gate combines it with controlled
hardware and Kronecker capacity evidence.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
import urllib.request
from datetime import datetime, timezone
from pathlib import Path


REQUIRED_FAMILIES = {"scale-free-web", "scale-free-social", "road-network"}
SIGNATURE_FIELDS = ("vertices", "edge_entries", "reachable_vertices", "component_count", "triangles")
TIMING_FIELDS = (
    "load_us", "bfs_us", "bfs_end_to_end_us", "components_us",
    "components_end_to_end_us", "triangle_us", "triangle_end_to_end_us",
    "pagerank_us", "pagerank_end_to_end_us",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def percentile95(values: list[int]) -> int:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(0.95 * (len(ordered) - 1) + 0.5))]


def capture(command: list[str]) -> str:
    try:
        return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()
    except Exception as exc:  # noqa: BLE001 - metadata collection must remain diagnostic
        return f"unavailable: {exc}"


def prepare_dataset(entry: dict, data_dir: Path) -> tuple[Path, dict]:
    name = entry["name"]
    expected_hash = entry["sha256"]
    if len(expected_hash) != 64 or any(c not in "0123456789abcdef" for c in expected_hash):
        raise ValueError(f"{name}: invalid SHA-256 pin")

    suffix = ".txt.gz" if entry.get("compression") == "gzip" else ".txt"
    source = data_dir / f"{name}{suffix}"
    normalized = data_dir / f"{name}.edges"
    if not source.exists():
        with urllib.request.urlopen(entry["url"]) as response, source.open("wb") as output:
            shutil.copyfileobj(response, output)
    actual_hash = sha256_file(source)
    if actual_hash != expected_hash:
        source.unlink(missing_ok=True)
        raise RuntimeError(f"{name}: source SHA-256 mismatch: expected {expected_hash}, got {actual_hash}")

    opener = gzip.open if entry.get("compression") == "gzip" else open
    vertices: set[int] = set()
    rows = 0
    with opener(source, "rt", encoding="utf-8") as src, normalized.open("w", encoding="utf-8") as dst:
        for line in src:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            fields = stripped.split()
            if len(fields) < 2:
                raise RuntimeError(f"{name}: malformed edge row {rows + 1}")
            u, v = int(fields[0]), int(fields[1])
            if u == v and entry.get("drop_self_loops", True):
                continue
            dst.write(f"{u} {v}\n")
            vertices.add(u)
            vertices.add(v)
            rows += 1

    expected_rows = int(entry["expected_normalized_rows"])
    expected_vertices = int(entry["expected_vertices"])
    if rows != expected_rows or len(vertices) != expected_vertices:
        raise RuntimeError(
            f"{name}: normalized statistics mismatch: vertices={len(vertices)} rows={rows}; "
            f"expected vertices={expected_vertices} rows={expected_rows}"
        )
    metadata = {
        "name": name,
        "family": entry["family"],
        "directed": bool(entry["directed"]),
        "source_url": entry["url"],
        "source_sha256": actual_hash,
        "normalized_sha256": sha256_file(normalized),
        "vertices_observed": len(vertices),
        "normalized_rows_observed": rows,
    }
    (data_dir / f"{name}.metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return normalized, metadata


def read_benchmark_row(path: Path) -> dict[str, str]:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 1:
        raise RuntimeError(f"expected exactly one benchmark row in {path}, got {len(rows)}")
    return rows[0]


def run_sample(binary: Path, edge_list: Path, source: int, output_dir: Path, label: str, keep: bool) -> dict:
    csv_path = output_dir / f"{label}.csv"
    stderr_path = output_dir / f"{label}.stderr.txt"
    resource_path = output_dir / f"{label}.resources.json"
    helper = Path(__file__).resolve().parents[1] / "tools" / "run_resource_measurement.py"
    command = [sys.executable, str(helper), "--output", str(resource_path), "--", str(binary), str(edge_list), str(source)]
    with csv_path.open("w", encoding="utf-8") as stdout, stderr_path.open("w", encoding="utf-8") as stderr:
        completed = subprocess.run(command, text=True, stdout=stdout, stderr=stderr, check=False)
    if completed.returncode != 0:
        raise RuntimeError(f"benchmark failed ({completed.returncode}); see {stderr_path}")
    row = read_benchmark_row(csv_path)
    resource_report = json.loads(resource_path.read_text(encoding="utf-8"))
    sample = {
        "label": label,
        "resource_measurement": resource_report,
        "peak_rss_bytes": int(resource_report["peak_rss_bytes"]),
        "signature": {field: row[field] for field in SIGNATURE_FIELDS},
        "pagerank_sum": float(row["pagerank_sum"]),
        "timings_us": {field: int(row[field]) for field in TIMING_FIELDS},
    }
    if not keep:
        csv_path.unlink(missing_ok=True)
        stderr_path.unlink(missing_ok=True)
        resource_path.unlink(missing_ok=True)
    return sample


def main() -> int:
    parser = argparse.ArgumentParser(description="Run checksum-pinned canonical real-dataset campaign")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--benchmark-binary", type=Path, required=True)
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=10)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--minimum-repeats", type=int, default=10)
    parser.add_argument("--minimum-warmups", type=int, default=2)
    parser.add_argument("--keep-edge-files", action="store_true")
    parser.add_argument("--keep-warmups", action="store_true")
    args = parser.parse_args()

    if args.repeats < 1 or args.warmups < 0 or args.minimum_repeats < 1 or args.minimum_warmups < 0:
        raise ValueError("repeat/warmup values must be positive/non-negative")
    binary = args.benchmark_binary.resolve()
    if not binary.is_file():
        raise ValueError(f"benchmark binary not found: {binary}")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported manifest schema_version")
    datasets = manifest.get("datasets")
    if not isinstance(datasets, list) or not datasets:
        raise ValueError("manifest datasets must be a non-empty list")
    families = {entry.get("family") for entry in datasets}
    if not REQUIRED_FAMILIES.issubset(families):
        raise ValueError(f"manifest missing required families: {sorted(REQUIRED_FAMILIES - families)}")

    args.data_dir.mkdir(parents=True, exist_ok=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    environment = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "git_head": capture(["git", "rev-parse", "HEAD"]),
        "git_status": capture(["git", "status", "--porcelain"]),
        "platform": platform.platform(),
        "hostname": platform.node(),
        "python": sys.version,
        "lscpu": capture(["lscpu"]),
        "numactl": capture(["numactl", "--hardware"]) if shutil.which("numactl") else "numactl unavailable",
        "memory": Path("/proc/meminfo").read_text(encoding="utf-8") if Path("/proc/meminfo").exists() else "unavailable",
        "thread_environment": {key: os.environ.get(key) for key in ("OMP_NUM_THREADS", "OMP_PLACES", "OMP_PROC_BIND")},
        "repeats": args.repeats,
        "warmups": args.warmups,
    }
    (args.output_dir / "environment.json").write_text(json.dumps(environment, indent=2) + "\n", encoding="utf-8")

    results = []
    for entry in datasets:
        edge_list, metadata = prepare_dataset(entry, args.data_dir)
        source = int(entry.get("source", 0))
        for index in range(args.warmups):
            run_sample(binary, edge_list, source, args.output_dir, f"{entry['name']}.warmup-{index + 1}", args.keep_warmups)
        samples = [
            run_sample(binary, edge_list, source, args.output_dir, f"{entry['name']}.sample-{index + 1}", True)
            for index in range(args.repeats)
        ]
        expected_signature = samples[0]["signature"]
        if any(sample["signature"] != expected_signature for sample in samples):
            raise RuntimeError(f"{entry['name']}: deterministic result signature mismatch")
        rank_sums = [sample["pagerank_sum"] for sample in samples]
        if max(rank_sums) - min(rank_sums) > 1e-9:
            raise RuntimeError(f"{entry['name']}: PageRank result changed across repetitions")
        timing_summary = {}
        for field in TIMING_FIELDS:
            values = [sample["timings_us"][field] for sample in samples]
            timing_summary[field] = {
                "median": statistics.median(values),
                "p95": percentile95(values),
                "min": min(values),
                "max": max(values),
            }
        results.append({
            "dataset": metadata,
            "source": source,
            "repeats": args.repeats,
            "warmups": args.warmups,
            "all_samples_consistent": True,
            "signature": expected_signature,
            "pagerank_sum": rank_sums[0],
            "peak_rss_bytes": {
                "median": statistics.median(sample["peak_rss_bytes"] for sample in samples),
                "max": max(sample["peak_rss_bytes"] for sample in samples),
            },
            "timing_summary_us": timing_summary,
            "samples": samples,
        })
        if not args.keep_edge_files:
            edge_list.unlink(missing_ok=True)

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-canonical-real-dataset-campaign",
        "research_claim": False,
        "publication_ready": False,
        "required_families": sorted(REQUIRED_FAMILIES),
        "covered_families": sorted(families),
        "minimum_repeats_met": args.repeats >= args.minimum_repeats,
        "minimum_warmups_met": args.warmups >= args.minimum_warmups,
        "all_results_consistent": all(item["all_samples_consistent"] for item in results),
        "results": results,
    }
    summary["canonical_real_dataset_gate_passed"] = bool(
        summary["minimum_repeats_met"] and summary["minimum_warmups_met"] and summary["all_results_consistent"]
    )
    (args.output_dir / "canonical-real-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "output": str(args.output_dir / "canonical-real-summary.json"),
        "datasets": [entry["name"] for entry in datasets],
        "canonical_real_dataset_gate_passed": summary["canonical_real_dataset_gate_passed"],
    }, sort_keys=True))
    return 0 if summary["canonical_real_dataset_gate_passed"] else 3


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
