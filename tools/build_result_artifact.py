#!/usr/bin/env python3
"""Build a provenance-rich machine-readable experiment artifact manifest.

The tool does not create performance claims. It packages already-produced
benchmark result files together with immutable hashes and explicit dataset,
hardware, software and experiment metadata so research-scale runs can be
archived and compared reproducibly.
"""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

SCHEMA_VERSION = 1


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def git_revision() -> str | None:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True, stderr=subprocess.DEVNULL
        ).strip()
    except Exception:
        return None


def read_optional_json(path: Path | None):
    if path is None:
        return None
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError(f"metadata file must contain a JSON object: {path}")
    return payload


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bundle benchmark outputs into a provenance-rich publication artifact manifest."
    )
    parser.add_argument("--experiment", required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--dataset-sha256", required=True)
    parser.add_argument("--result", type=Path, action="append", required=True)
    parser.add_argument("--hardware-json", type=Path)
    parser.add_argument("--software-json", type=Path)
    parser.add_argument("--notes")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    expected_dataset_hash = args.dataset_sha256.lower()
    if len(expected_dataset_hash) != 64 or any(c not in "0123456789abcdef" for c in expected_dataset_hash):
        raise ValueError("--dataset-sha256 must be a 64-character lowercase hexadecimal SHA-256")

    results = []
    for path in args.result:
        if not path.is_file():
            raise ValueError(f"result file does not exist: {path}")
        results.append(
            {
                "path": str(path),
                "sha256": sha256_file(path),
                "bytes": path.stat().st_size,
            }
        )

    artifact = {
        "schema_version": SCHEMA_VERSION,
        "artifact_type": "velographx-experiment-results",
        "research_claim": False,
        "experiment": args.experiment,
        "dataset": {
            "name": args.dataset,
            "sha256": expected_dataset_hash,
        },
        "results": results,
        "provenance": {
            "git_revision": git_revision(),
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "cpu_count": os.cpu_count(),
        },
        "hardware": read_optional_json(args.hardware_json),
        "software": read_optional_json(args.software_json),
        "notes": args.notes,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(artifact, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
