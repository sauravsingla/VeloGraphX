#!/usr/bin/env python3
"""Validate competitor version pins before publication-grade benchmark execution.

The validator is intentionally conservative. It allows an unresolved research
plan to exist, but any competitor marked ready must have an immutable identity,
an install/build recipe, and (when an environment manifest is supplied) a
matching installed/captured identity. It never invents or selects versions.
"""

import argparse
import json
import re
import sys
from pathlib import Path

FLOATING = {"latest", "main", "master", "head", "tip", "stable"}
HEX_SHA = re.compile(r"^[0-9a-fA-F]{7,64}$")
EXACT_VERSION = re.compile(r"^v?\d+(?:\.\d+)+(?:[-+._a-zA-Z0-9]*)?$")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def immutable_pin(value: str) -> bool:
    pin = value.strip()
    if not pin or pin.lower() in FLOATING:
        return False
    return bool(HEX_SHA.fullmatch(pin) or EXACT_VERSION.fullmatch(pin))


def load_object(path: Path) -> dict:
    payload = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(payload, dict), f"JSON root must be an object: {path}")
    return payload


def env_identity(environment: dict, competitor: dict):
    name = competitor["name"]
    python_map = {
        "NetworkX": "networkx",
        "python-igraph": "igraph",
        "NetworKit": "networkit",
        "rustworkx": "rustworkx",
    }
    native_map = {
        "SuiteSparse:GraphBLAS/LAGraph": "lagraph_bfs",
        "GAP Benchmark Suite": "gap_bfs",
    }
    if name in python_map:
        version = environment.get("python_competitors", {}).get(python_map[name])
        return None if version is None else str(version)
    if name in native_map:
        native = environment.get("native_competitors", {}).get(native_map[name], {})
        if not isinstance(native, dict) or native.get("available") is not True:
            return None
        for key in ("version_or_commit", "version", "commit", "version_output"):
            value = native.get(key)
            if value:
                return str(value)
        return None
    return None


def validate(plan: dict, environment: dict | None) -> dict:
    require(plan.get("schema_version") == 1, "schema_version must be 1")
    require(plan.get("artifact_type") == "velographx-competitor-research-plan",
            "unexpected artifact_type")
    require(plan.get("research_claim") is False, "research plan must have research_claim=false")
    competitors = plan.get("competitors")
    require(isinstance(competitors, list) and competitors, "competitors must be a non-empty array")

    seen = set()
    ready_count = 0
    unresolved_count = 0
    for index, competitor in enumerate(competitors):
        require(isinstance(competitor, dict), f"competitors[{index}] must be an object")
        for key in ("name", "kind", "version_or_commit", "source", "install_or_build_recipe", "ready"):
            require(key in competitor, f"competitors[{index}] missing field: {key}")
        name = competitor["name"]
        require(isinstance(name, str) and name.strip(), f"competitors[{index}].name must be non-empty")
        require(name not in seen, f"duplicate competitor name: {name}")
        seen.add(name)
        require(isinstance(competitor["ready"], bool), f"{name}.ready must be boolean")

        pin = competitor["version_or_commit"]
        if competitor["ready"]:
            ready_count += 1
            require(isinstance(pin, str) and immutable_pin(pin),
                    f"{name} ready=true requires an immutable version_or_commit")
            recipe = competitor["install_or_build_recipe"]
            require(isinstance(recipe, str) and recipe.strip(),
                    f"{name} ready=true requires install_or_build_recipe")
            if environment is not None:
                captured = env_identity(environment, competitor)
                require(captured is not None, f"{name} ready=true but environment identity is unavailable")
                require(pin.lower() in captured.lower() or captured.lower() in pin.lower(),
                        f"{name} pin does not match captured environment identity")
        else:
            unresolved_count += 1
            if isinstance(pin, str):
                require(pin.lower() not in FLOATING,
                        f"{name} uses forbidden floating version label: {pin}")

    if environment is not None:
        require(environment.get("schema_version") == 1, "environment schema_version must be 1")
        require(environment.get("artifact_type") == "velographx-benchmark-environment",
                "unexpected environment artifact_type")
        require(environment.get("research_claim") is False,
                "benchmark environment must have research_claim=false")

    return {
        "valid": True,
        "competitor_count": len(competitors),
        "ready_count": ready_count,
        "unresolved_count": unresolved_count,
        "publication_ready": ready_count == len(competitors),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate VeloGraphX competitor research readiness")
    parser.add_argument("plan", type=Path)
    parser.add_argument("--environment", type=Path)
    args = parser.parse_args()

    plan = load_object(args.plan)
    environment = load_object(args.environment) if args.environment else None
    report = validate(plan, environment)
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
