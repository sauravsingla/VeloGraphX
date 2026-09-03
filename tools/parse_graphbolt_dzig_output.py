#!/usr/bin/env python3
"""Parse the pinned DZiG artifact's native stdout into a strict JSON record."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

VALUE = r"([0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)"
PATTERNS = {
    "reading_seconds": re.compile(r"^Reading Time\s*:\s*" + VALUE + r"\s*$", re.I),
    "deletion_seconds": re.compile(r"^Edge deletion time\s*:\s*" + VALUE + r"\s*$", re.I),
    "addition_seconds": re.compile(r"^Edge addition time\s*:\s*" + VALUE + r"\s*$", re.I),
    "compute_seconds": re.compile(r"^Finished batch\s*:\s*" + VALUE + r"\s*$", re.I),
    "additions": re.compile(r"^Edge Additions in batch\s*:\s*([0-9]+)\s*$", re.I),
    "deletions": re.compile(r"^Edge Deletions in batch\s*:\s*([0-9]+)\s*$", re.I),
    "edges_processed": re.compile(r"^Edges [Pp]rocessed\s*:\s*([0-9]+)\s*$"),
}


def parse(text: str):
    batches, current = [], {}
    for raw in text.splitlines():
        line = raw.strip()
        for name, pattern in PATTERNS.items():
            match = pattern.match(line)
            if not match:
                continue
            if name == "edges_processed" and batches and not current:
                batches[-1][name] = int(match.group(1))
                break
            if name == "reading_seconds" and current:
                raise ValueError("new batch began before the preceding batch was complete")
            current[name] = int(match.group(1)) if name in {"additions", "deletions", "edges_processed"} else float(match.group(1))
            if name == "compute_seconds":
                required = {"reading_seconds", "deletion_seconds", "addition_seconds", "compute_seconds"}
                missing = required - current.keys()
                if missing:
                    raise ValueError("incomplete GraphBolt batch: " + ", ".join(sorted(missing)))
                current["mutation_seconds"] = current["deletion_seconds"] + current["addition_seconds"]
                current["answer_ready_seconds"] = current["mutation_seconds"] + current["compute_seconds"]
                batches.append(current)
                current = {}
            break
    if current:
        raise ValueError("truncated GraphBolt batch output")
    if not batches:
        raise ValueError("no complete GraphBolt/DZiG batch found")
    return batches


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-batches", type=int)
    parser.add_argument("--expected-operations", type=int)
    parser.add_argument("--require-work", action="store_true")
    args = parser.parse_args()
    batches = parse(args.input.read_text(encoding="utf-8"))
    if args.expected_batches is not None and len(batches) != args.expected_batches:
        raise ValueError(f"expected {args.expected_batches} batches, parsed {len(batches)}")
    for index, batch in enumerate(batches, 1):
        if args.expected_operations is not None:
            observed = batch.get("additions", 0) + batch.get("deletions", 0)
            if observed != args.expected_operations:
                raise ValueError(f"batch {index}: expected {args.expected_operations} valid operations, observed {observed}")
        if args.require_work and "edges_processed" not in batch:
            raise ValueError(f"batch {index}: EDGEWORK output is missing")
    artifact = {
        "schema_version": 1,
        "artifact_type": "graphbolt-dzig-native-output",
        "timing_contract": {
            "mutation_seconds": "Edge deletion time + Edge addition time",
            "compute_seconds": "Finished batch",
            "answer_ready_seconds": "mutation_seconds + compute_seconds",
            "reading_seconds_excluded": True,
        },
        "batch_count": len(batches), "batches": batches,
        "correctness_verified": False, "publishable": False, "research_claim": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(artifact, indent=2, sort_keys=True) + "\n")
    print(json.dumps(artifact, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
