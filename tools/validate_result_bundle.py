#!/usr/bin/env python3
"""Validate VeloGraphX publication result artifacts.

This validator is deliberately aligned with tools/build_result_artifact.py so
there is one archival format. It verifies structure, provenance fields and
referenced result/preflight hashes. It does not certify scientific validity or
turn CI/synthetic data into a research claim.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def resolve_file(path_value: str, base_dir: Path) -> Path | None:
    configured = Path(path_value)
    candidates = [configured] if configured.is_absolute() else [Path.cwd() / configured, base_dir / configured]
    return next((candidate.resolve() for candidate in candidates if candidate.is_file()), None)


def validate_hashed_file(item: dict, label: str, base_dir: Path) -> Path:
    require(isinstance(item, dict), f"{label} must be an object")
    path_value = item.get("path")
    expected_hash = item.get("sha256")
    expected_bytes = item.get("bytes")
    require(isinstance(path_value, str) and path_value, f"{label}.path must be non-empty")
    require(isinstance(expected_hash, str) and len(expected_hash) == 64 and
            all(c in "0123456789abcdefABCDEF" for c in expected_hash),
            f"{label}.sha256 must be a hexadecimal SHA-256")
    require(isinstance(expected_bytes, int) and expected_bytes >= 0,
            f"{label}.bytes must be a non-negative integer")
    existing = resolve_file(path_value, base_dir)
    require(existing is not None, f"{label} file does not exist: {path_value}")
    require(existing.stat().st_size == expected_bytes, f"{label} byte-size mismatch: {path_value}")
    require(sha256_file(existing).lower() == expected_hash.lower(),
            f"{label} sha256 mismatch: {path_value}")
    return existing


def validate(payload: dict, manifest_path: Path) -> None:
    require(isinstance(payload, dict), "artifact must be a JSON object")
    require(payload.get("schema_version") == 1, "schema_version must be 1")
    require(payload.get("artifact_type") == "velographx-experiment-results",
            "artifact_type must be velographx-experiment-results")
    require(isinstance(payload.get("research_claim"), bool), "research_claim must be boolean")
    require(isinstance(payload.get("experiment"), str) and payload["experiment"].strip(),
            "experiment must be non-empty")

    dataset = payload.get("dataset")
    require(isinstance(dataset, dict), "dataset must be an object")
    require(isinstance(dataset.get("name"), str) and dataset["name"].strip(),
            "dataset.name must be non-empty")
    dataset_hash = dataset.get("sha256")
    require(isinstance(dataset_hash, str) and len(dataset_hash) == 64 and
            all(c in "0123456789abcdefABCDEF" for c in dataset_hash),
            "dataset.sha256 must be a 64-character hexadecimal SHA-256")

    provenance = payload.get("provenance")
    require(isinstance(provenance, dict), "provenance must be an object")
    revision = provenance.get("git_revision")
    require(revision is None or (isinstance(revision, str) and revision.strip()),
            "provenance.git_revision must be null or non-empty")
    require(isinstance(provenance.get("platform"), str) and provenance["platform"],
            "provenance.platform must be non-empty")
    require(isinstance(provenance.get("machine"), str), "provenance.machine must be a string")
    require(isinstance(provenance.get("python"), str) and provenance["python"],
            "provenance.python must be non-empty")

    base_dir = manifest_path.parent
    results = payload.get("results")
    require(isinstance(results, list) and results, "results must be a non-empty array")
    for index, result in enumerate(results):
        validate_hashed_file(result, f"results[{index}]", base_dir)

    preflight = payload.get("preflight")
    if preflight is not None:
        preflight_path = validate_hashed_file(preflight, "preflight", base_dir)
        require(preflight.get("valid") is True, "preflight.valid must be true")
        require(preflight.get("research_claim") is False,
                "preflight.research_claim must be false")
        preflight_payload = json.loads(preflight_path.read_text(encoding="utf-8"))
        require(isinstance(preflight_payload, dict), "preflight file must contain a JSON object")
        require(preflight_payload.get("valid") is True,
                "bound preflight file must have valid=true")
        require(preflight_payload.get("research_claim") is False,
                "bound preflight file must keep research_claim=false")
        if "artifact_type" in preflight and preflight.get("artifact_type") is not None:
            require(preflight_payload.get("artifact_type") == preflight.get("artifact_type"),
                    "bound preflight artifact_type mismatch")

    # build_result_artifact.py intentionally emits false. If a future artifact
    # format permits true, it must first define stronger dedicated-hardware and
    # measurement-provenance requirements rather than silently accepting it.
    require(payload["research_claim"] is False,
            "schema v1 artifacts must keep research_claim=false")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a VeloGraphX publication result artifact")
    parser.add_argument("artifact", type=Path)
    args = parser.parse_args()

    artifact_path = args.artifact.resolve()
    payload = json.loads(artifact_path.read_text(encoding="utf-8"))
    validate(payload, artifact_path)
    print(json.dumps({"valid": True, "schema_version": 1, "research_claim": False}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
