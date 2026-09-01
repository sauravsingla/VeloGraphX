#!/usr/bin/env python3
"""Combine canonical real, Kronecker capacity, and hardware evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--real", type=Path, required=True)
    parser.add_argument("--capacity", type=Path, required=True)
    parser.add_argument("--hardware", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expect-publication-ready", action="store_true")
    args = parser.parse_args()

    real = json.loads(args.real.read_text(encoding="utf-8"))
    capacity = json.loads(args.capacity.read_text(encoding="utf-8"))
    hardware = json.loads(args.hardware.read_text(encoding="utf-8"))
    checks = {
        "scale_free_web_covered": "scale-free-web" in real.get("covered_families", []),
        "scale_free_social_covered": "scale-free-social" in real.get("covered_families", []),
        "road_network_covered": "road-network" in real.get("covered_families", []),
        "real_dataset_repetitions_valid": real.get("canonical_real_dataset_gate_passed") is True,
        "kronecker_series_executed": len(capacity.get("records", [])) >= 2,
        "largest_in_memory_boundary_established": capacity.get("boundary_established") is True,
        "controlled_hardware_measurements_valid": hardware.get("hardware_measurement_ready") is True,
        "hardware_counter_case_present": any(case.get("kind") == "hardware_counters" for case in hardware.get("cases", [])),
    }
    ready = all(checks.values())
    report = {
        "schema_version": 1,
        "artifact_type": "velographx-canonical-publication-campaign",
        "research_claim": ready,
        "publication_ready": ready,
        "allow_publication_claims": ready,
        "checks": checks,
        "real_dataset_summary": str(args.real),
        "capacity_summary": str(args.capacity),
        "hardware_summary": str(args.hardware),
        "largest_accepted_kronecker_scale": capacity.get("largest_accepted_scale"),
        "first_rejected_kronecker_scale": capacity.get("first_rejected_scale"),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    if args.expect_publication_ready and not ready:
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
