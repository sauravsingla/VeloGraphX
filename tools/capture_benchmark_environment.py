#!/usr/bin/env python3
"""Capture a machine-readable benchmark environment manifest.

This tool records provenance only. It does not run benchmarks or make performance
claims. Missing optional competitors are recorded as unavailable rather than
silently substituted.
"""

import argparse
import importlib.metadata
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

SCHEMA_VERSION = 1
PYTHON_PACKAGES = ("networkx", "igraph", "networkit", "rustworkx")
NATIVE_ENV = {
    "lagraph_bfs": "VELOGRAPHX_LAGRAPH_BFS_BIN",
    "gap_bfs": "VELOGRAPHX_GAP_BFS_BIN",
}


def command_version(command: str | None):
    if not command:
        return None
    resolved = shutil.which(command) if os.sep not in command else command
    if not resolved or not Path(resolved).is_file():
        return {"configured": command, "available": False}
    for args in ([resolved, "--version"], [resolved, "-version"], [resolved, "-v"]):
        try:
            proc = subprocess.run(args, text=True, capture_output=True, timeout=5, check=False)
            text = (proc.stdout or proc.stderr).strip()
            if text:
                return {
                    "configured": command,
                    "resolved": str(Path(resolved).resolve()),
                    "available": True,
                    "version_output": text.splitlines()[0],
                }
        except Exception:
            pass
    return {
        "configured": command,
        "resolved": str(Path(resolved).resolve()),
        "available": True,
        "version_output": None,
    }


def git_revision():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True, stderr=subprocess.DEVNULL
        ).strip()
    except Exception:
        return None


def package_version(name: str):
    try:
        return importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture VeloGraphX benchmark environment metadata")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--label", default="benchmark-environment")
    args = parser.parse_args()

    compilers = {name: command_version(name) for name in ("c++", "g++", "clang++")}

    native = {}
    for name, env_name in NATIVE_ENV.items():
        configured = os.environ.get(env_name)
        details = command_version(configured)
        if details is None:
            details = {"configured": None, "available": False}
        details["environment_variable"] = env_name
        native[name] = details

    manifest = {
        "schema_version": SCHEMA_VERSION,
        "artifact_type": "velographx-benchmark-environment",
        "research_claim": False,
        "label": args.label,
        "velographx": {"git_revision": git_revision()},
        "hardware": {
            "machine": platform.machine(),
            "processor": platform.processor(),
            "cpu_count": os.cpu_count(),
        },
        "operating_system": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "platform": platform.platform(),
        },
        "runtime": {"python": platform.python_version()},
        "compilers": compilers,
        "python_competitors": {name: package_version(name) for name in PYTHON_PACKAGES},
        "native_competitors": native,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
