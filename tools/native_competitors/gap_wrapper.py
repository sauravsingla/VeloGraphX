#!/usr/bin/env python3
"""Adapter shim for a locally built GAP Benchmark Suite BFS runner.

Set VELOGRAPHX_GAP_BFS_BIN to an executable that accepts:

  --dataset PATH --source N --vertices N [--directed]

and prints one JSON object containing at least a `distances` array. The shim
normalizes that runner to VeloGraphX's external competitor contract and fails
explicitly when the native dependency is unavailable.
"""

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


def require_env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise RuntimeError(f"{name} is required")
    return value


def main() -> int:
    if os.environ.get("VELOGRAPHX_ALGORITHM") != "bfs":
        raise RuntimeError("GAP wrapper currently supports only bfs")
    dataset = Path(require_env("VELOGRAPHX_DATASET"))
    source = int(require_env("VELOGRAPHX_SOURCE"))
    vertices = int(require_env("VELOGRAPHX_VERTICES"))
    directed = require_env("VELOGRAPHX_DIRECTED") == "1"
    if not dataset.is_file():
        raise RuntimeError(f"dataset does not exist: {dataset}")

    configured = require_env("VELOGRAPHX_GAP_BFS_BIN")
    binary = shutil.which(configured) if os.sep not in configured else configured
    if not binary or not Path(binary).is_file():
        raise RuntimeError(f"GAP BFS runner not found: {configured}")

    argv = [binary, "--dataset", str(dataset), "--source", str(source), "--vertices", str(vertices)]
    if directed:
        argv.append("--directed")
    proc = subprocess.run(argv, text=True, capture_output=True, check=False)
    if proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip() or f"exit code {proc.returncode}"
        raise RuntimeError(f"GAP BFS runner failed: {detail}")
    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError("GAP BFS runner must emit one JSON object") from exc
    if not isinstance(payload, dict) or "distances" not in payload:
        raise RuntimeError("GAP BFS runner JSON must contain distances")
    if len(payload["distances"]) != vertices:
        raise RuntimeError("GAP BFS runner returned the wrong number of distances")
    payload.setdefault("framework_version", "GAP-local")
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
