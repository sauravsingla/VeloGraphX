#!/usr/bin/env python3
"""Validate that a benchmark campaign is ready for reproducible execution.

This tool performs preflight checks only. It does not run benchmarks and does
not make or upgrade research claims.
"""

import argparse
import json
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def load_json(path: Path) -> dict:
    payload = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(payload, dict), f"JSON must contain an object: {path}")
    return payload


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate VeloGraphX benchmark campaign readiness")
    parser.add_argument("--environment", type=Path, required=True)
    parser.add_argument("--dataset-manifest", type=Path, required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--require-native", action="append", choices=["lagraph_bfs", "gap_bfs"], default=[])
    parser.add_argument("--require-dedicated-hardware", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    env = load_json(args.environment)
    manifest = load_json(args.dataset_manifest)

    require(env.get("schema_version") == 1, "environment schema_version must be 1")
    require(env.get("artifact_type") == "velographx-benchmark-environment", "invalid environment artifact_type")
    require(env.get("research_claim") is False, "environment capture must not be a research claim")

    revision = (env.get("velographx") or {}).get("git_revision")
    require(isinstance(revision, str) and len(revision) >= 7, "environment must record a VeloGraphX git revision")

    hardware = env.get("hardware")
    require(isinstance(hardware, dict), "environment.hardware must be an object")
    require(isinstance(hardware.get("cpu_count"), int) and hardware["cpu_count"] > 0, "environment must record cpu_count")
    require(isinstance(hardware.get("machine"), str) and hardware["machine"], "environment must record machine architecture")

    if args.require_dedicated_hardware:
        require(hardware.get("dedicated") is True, "dedicated-hardware campaign requires hardware.dedicated=true")
        require(isinstance(hardware.get("model"), str) and hardware["model"].strip(), "dedicated hardware requires hardware.model")
        require(isinstance(hardware.get("memory_bytes"), int) and hardware["memory_bytes"] > 0, "dedicated hardware requires hardware.memory_bytes")

    native = env.get("native_competitors") or {}
    for name in args.require_native:
        item = native.get(name)
        require(isinstance(item, dict), f"environment missing native competitor: {name}")
        require(item.get("available") is True, f"required native competitor is unavailable: {name}")
        require(isinstance(item.get("resolved"), str) and item["resolved"], f"required native competitor lacks resolved binary: {name}")

    datasets = manifest.get("datasets")
    require(isinstance(datasets, list), "dataset manifest must contain a datasets array")
    matches = [item for item in datasets if isinstance(item, dict) and item.get("name") == args.dataset]
    require(len(matches) == 1, f"dataset must appear exactly once in manifest: {args.dataset}")
    dataset = matches[0]
    digest = dataset.get("sha256")
    require(isinstance(digest, str) and len(digest) == 64 and all(c in "0123456789abcdefABCDEF" for c in digest),
            "dataset entry must contain a valid SHA-256")

    report = {
        "schema_version": 1,
        "artifact_type": "velographx-campaign-readiness",
        "ready": True,
        "research_claim": False,
        "git_revision": revision,
        "dataset": {"name": args.dataset, "sha256": digest.lower()},
        "required_native_competitors": args.require_native,
        "dedicated_hardware_required": args.require_dedicated_hardware,
    }

    text = json.dumps(report, sort_keys=True, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
