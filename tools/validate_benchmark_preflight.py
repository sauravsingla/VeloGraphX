#!/usr/bin/env python3
"""Validate prerequisites for dedicated-hardware benchmark campaigns.

This tool checks provenance and dependency readiness only. It does not run
benchmarks or make performance claims.
"""

import argparse
import json
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def load_object(path: Path, label: str) -> dict:
    require(path.is_file(), f"{label} file does not exist: {path}")
    payload = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(payload, dict), f"{label} must contain a JSON object")
    return payload


def validate_environment(env: dict, require_native: bool) -> None:
    require(env.get("schema_version") == 1, "environment schema_version must be 1")
    require(env.get("artifact_type") == "velographx-benchmark-environment",
            "environment artifact_type mismatch")
    require(env.get("research_claim") is False,
            "environment manifest must not itself be a research claim")
    revision = env.get("velographx", {}).get("git_revision")
    require(isinstance(revision, str) and len(revision) >= 7,
            "environment must record VeloGraphX git revision")
    hardware = env.get("hardware")
    require(isinstance(hardware, dict), "environment.hardware must be an object")
    require(isinstance(hardware.get("cpu_count"), int) and hardware["cpu_count"] > 0,
            "environment.hardware.cpu_count must be positive")
    require(isinstance(hardware.get("machine"), str) and hardware["machine"],
            "environment.hardware.machine must be recorded")

    native = env.get("native_competitors", {})
    if require_native:
        for key in ("lagraph_bfs", "gap_bfs"):
            item = native.get(key)
            require(isinstance(item, dict), f"native competitor metadata missing: {key}")
            require(item.get("available") is True,
                    f"required native competitor unavailable: {key}")
            require(isinstance(item.get("resolved"), str) and item["resolved"],
                    f"required native competitor has no resolved binary: {key}")


def validate_dataset(dataset: dict) -> None:
    require(dataset.get("schema_version") == 1, "dataset report schema_version must be 1")
    prepared = dataset.get("prepared")
    require(isinstance(prepared, list) and prepared,
            "dataset report must contain at least one prepared dataset")
    for index, item in enumerate(prepared):
        require(isinstance(item, dict), f"prepared[{index}] must be an object")
        require(isinstance(item.get("name"), str) and item["name"],
                f"prepared[{index}].name must be non-empty")
        sha = item.get("sha256")
        require(isinstance(sha, str) and len(sha) == 64 and all(c in "0123456789abcdefABCDEF" for c in sha),
                f"prepared[{index}].sha256 must be a SHA-256")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate benchmark campaign preflight prerequisites")
    parser.add_argument("--environment", type=Path, required=True)
    parser.add_argument("--dataset-report", type=Path, required=True)
    parser.add_argument("--require-native-competitors", action="store_true")
    args = parser.parse_args()

    environment = load_object(args.environment, "environment")
    dataset = load_object(args.dataset_report, "dataset report")
    validate_environment(environment, args.require_native_competitors)
    validate_dataset(dataset)

    print(json.dumps({
        "valid": True,
        "research_claim": False,
        "native_competitors_required": args.require_native_competitors,
        "dataset_count": len(dataset["prepared"]),
        "git_revision": environment["velographx"]["git_revision"],
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
