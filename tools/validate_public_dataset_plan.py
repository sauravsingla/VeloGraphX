#!/usr/bin/env python3
"""Validate public research dataset readiness metadata.

The validator is intentionally fail-closed: unresolved dataset slots are valid
as planning records but are not execution-ready. Use --require-ready to prevent
benchmark campaigns from starting until every required provenance field is set.
"""

import argparse
import json
import sys
from pathlib import Path

REQUIRED_FIELDS = (
    "canonical_source",
    "license_or_terms",
    "download_url",
    "sha256",
    "format",
    "directed",
    "vertex_count",
    "edge_count",
)


def is_sha256(value) -> bool:
    return isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdefABCDEF" for c in value)


def validate_slot(slot: dict) -> list[str]:
    errors = []
    for key in REQUIRED_FIELDS:
        if slot.get(key) is None or slot.get(key) == "":
            errors.append(f"{slot.get('id', '<unknown>')}: missing {key}")
    if slot.get("sha256") is not None and not is_sha256(slot.get("sha256")):
        errors.append(f"{slot.get('id', '<unknown>')}: sha256 must be 64 hexadecimal characters")
    for key in ("vertex_count", "edge_count"):
        value = slot.get(key)
        if value is not None and (not isinstance(value, int) or value < 0):
            errors.append(f"{slot.get('id', '<unknown>')}: {key} must be a non-negative integer")
    if slot.get("directed") is not None and not isinstance(slot.get("directed"), bool):
        errors.append(f"{slot.get('id', '<unknown>')}: directed must be boolean")
    ready = not errors
    if bool(slot.get("ready")) != ready:
        errors.append(f"{slot.get('id', '<unknown>')}: ready flag does not match provenance completeness")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a VeloGraphX public dataset research plan")
    parser.add_argument("plan", type=Path)
    parser.add_argument("--require-ready", action="store_true")
    args = parser.parse_args()

    payload = json.loads(args.plan.read_text(encoding="utf-8"))
    if payload.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")
    if payload.get("artifact_type") != "velographx-public-dataset-research-plan":
        raise ValueError("unexpected artifact_type")
    if payload.get("research_claim") is not False:
        raise ValueError("research_claim must be false")
    slots = payload.get("dataset_slots")
    if not isinstance(slots, list) or not slots:
        raise ValueError("dataset_slots must be a non-empty array")

    slot_errors = {slot.get("id", f"slot-{i}"): validate_slot(slot) for i, slot in enumerate(slots)}
    incomplete = {name: errs for name, errs in slot_errors.items() if errs}
    if args.require_ready and incomplete:
        details = "; ".join(f"{name}: {len(errs)} issue(s)" for name, errs in incomplete.items())
        raise ValueError(f"public dataset plan is not execution-ready: {details}")

    print(json.dumps({
        "valid_plan": True,
        "execution_ready": not incomplete,
        "slot_count": len(slots),
        "incomplete_slots": sorted(incomplete),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
