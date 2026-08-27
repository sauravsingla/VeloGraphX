#!/usr/bin/env python3
"""Validate publication-oriented VeloGraphX result bundles.

The validator checks provenance and structure only. It does not certify that a
benchmark result is scientifically valid, and it never upgrades synthetic or CI
fixtures into research claims.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path

REQUIRED_TOP_LEVEL = (
    "schema_version",
    "repository_commit",
    "campaign",
    "dataset",
    "environment",
    "results",
    "research_claim",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def validate(bundle: dict, base_dir: Path) -> None:
    require(isinstance(bundle, dict), "bundle must be a JSON object")
    for key in REQUIRED_TOP_LEVEL:
        require(key in bundle, f"missing required field: {key}")

    require(bundle["schema_version"] == 1, "schema_version must be 1")
    commit = bundle["repository_commit"]
    require(isinstance(commit, str) and 7 <= len(commit) <= 64, "repository_commit must be a commit SHA string")
    require(all(c in "0123456789abcdefABCDEF" for c in commit), "repository_commit must be hexadecimal")
    require(isinstance(bundle["campaign"], str) and bundle["campaign"].strip(), "campaign must be non-empty")
    require(isinstance(bundle["research_claim"], bool), "research_claim must be boolean")

    dataset = bundle["dataset"]
    require(isinstance(dataset, dict), "dataset must be an object")
    for key in ("name", "sha256"):
        require(isinstance(dataset.get(key), str) and dataset[key], f"dataset.{key} must be non-empty")
    require(len(dataset["sha256"]) == 64 and all(c in "0123456789abcdefABCDEF" for c in dataset["sha256"]),
            "dataset.sha256 must be a 64-character hexadecimal SHA-256")

    environment = bundle["environment"]
    require(isinstance(environment, dict) and environment, "environment must be a non-empty object")
    results = bundle["results"]
    require(isinstance(results, list) and results, "results must be a non-empty array")
    for index, result in enumerate(results):
        require(isinstance(result, dict), f"results[{index}] must be an object")
        require(isinstance(result.get("name"), str) and result["name"], f"results[{index}].name must be non-empty")
        require("metrics" in result and isinstance(result["metrics"], dict), f"results[{index}].metrics must be an object")

    artifacts = bundle.get("artifacts", [])
    require(isinstance(artifacts, list), "artifacts must be an array when provided")
    for index, artifact in enumerate(artifacts):
        require(isinstance(artifact, dict), f"artifacts[{index}] must be an object")
        path_value = artifact.get("path")
        expected = artifact.get("sha256")
        require(isinstance(path_value, str) and path_value, f"artifacts[{index}].path must be non-empty")
        require(isinstance(expected, str) and len(expected) == 64, f"artifacts[{index}].sha256 must be SHA-256")
        path = (base_dir / path_value).resolve()
        require(path.is_file(), f"artifact does not exist: {path_value}")
        actual = sha256_file(path)
        require(actual.lower() == expected.lower(), f"artifact sha256 mismatch: {path_value}")

    if bundle["research_claim"]:
        provenance = bundle.get("provenance")
        require(isinstance(provenance, dict), "research_claim=true requires provenance")
        require(provenance.get("dedicated_hardware") is True,
                "research_claim=true requires provenance.dedicated_hardware=true")
        require(isinstance(provenance.get("notes"), str) and provenance["notes"].strip(),
                "research_claim=true requires provenance.notes")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a VeloGraphX publication result bundle")
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()

    bundle_path = args.bundle.resolve()
    payload = json.loads(bundle_path.read_text(encoding="utf-8"))
    validate(payload, bundle_path.parent)
    print(json.dumps({"valid": True, "schema_version": 1, "research_claim": payload["research_claim"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
