#!/usr/bin/env python3
"""Prepare a timestamp-ordered temporal edge stream for selector holdout tests.

The output drops the timestamp column only after verifying that timestamps are
nondecreasing, so source order remains genuine temporal order. Vertex IDs are
dense-relabelled by ascending original ID. The downstream VeloGraphX sliding
window therefore models timestamp-ordered arrivals with explicit edge expiry;
expiry deletions are induced by the benchmark window, not observed deletions.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def digest(path: Path, name: str) -> str:
    h = hashlib.new(name)
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def rows(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        for lineno, raw in enumerate(handle, 1):
            line = raw.strip()
            if not line or line.startswith(("#", "%")):
                continue
            fields = line.split()
            if len(fields) < 3:
                raise ValueError(f"{path}:{lineno}: expected SRC DST UNIXTS")
            u, v, ts = map(int, fields[:3])
            if u < 0 or v < 0:
                raise ValueError(f"{path}:{lineno}: negative vertex id")
            yield lineno, u, v, ts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--report", type=Path, required=True)
    ap.add_argument("--expected-md5")
    ap.add_argument("--expected-sha256")
    ap.add_argument("--expected-vertices", type=int)
    ap.add_argument("--expected-events", type=int)
    args = ap.parse_args()

    source_md5 = digest(args.input, "md5")
    source_sha256 = digest(args.input, "sha256")
    if args.expected_md5 and source_md5 != args.expected_md5.lower():
        raise RuntimeError(f"source md5 {source_md5} != expected {args.expected_md5}")
    if args.expected_sha256 and source_sha256 != args.expected_sha256.lower():
        raise RuntimeError(f"source sha256 {source_sha256} != expected {args.expected_sha256}")

    vertices = set()
    event_count = 0
    self_loops = 0
    duplicate_consecutive_events = 0
    previous_ts = None
    previous_event = None
    min_ts = None
    max_ts = None
    for lineno, u, v, ts in rows(args.input):
        if previous_ts is not None and ts < previous_ts:
            raise RuntimeError(
                f"timestamp order decreases at line {lineno}: {ts} < {previous_ts}"
            )
        event = (u, v, ts)
        duplicate_consecutive_events += int(event == previous_event)
        previous_event = event
        previous_ts = ts
        vertices.update((u, v))
        event_count += 1
        self_loops += int(u == v)
        min_ts = ts if min_ts is None else min(min_ts, ts)
        max_ts = ts if max_ts is None else max(max_ts, ts)

    if args.expected_vertices is not None and len(vertices) != args.expected_vertices:
        raise RuntimeError(f"vertices {len(vertices)} != expected {args.expected_vertices}")
    if args.expected_events is not None and event_count != args.expected_events:
        raise RuntimeError(f"events {event_count} != expected {args.expected_events}")

    ordered = sorted(vertices)
    dense = {vertex: i for i, vertex in enumerate(ordered)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as out:
        for _, u, v, _ in rows(args.input):
            if u == v:
                continue
            out.write(f"{dense[u]}\t{dense[v]}\n")

    output_events = sum(1 for _ in args.output.open("r", encoding="utf-8"))
    report = {
        "schema_version": 1,
        "artifact_type": "velographx-temporal-edge-stream",
        "source_path": str(args.input),
        "source_md5": source_md5,
        "source_sha256": source_sha256,
        "source_vertices": len(vertices),
        "source_events": event_count,
        "source_self_loops_removed": self_loops,
        "source_consecutive_duplicate_events": duplicate_consecutive_events,
        "timestamp_order_verified_nondecreasing": True,
        "min_timestamp": min_ts,
        "max_timestamp": max_ts,
        "output_path": str(args.output),
        "output_events": output_events,
        "output_sha256": digest(args.output, "sha256"),
        "vertex_relabeling": "ascending original vertex ID -> contiguous 0..N-1",
        "temporal_semantics": (
            "arrival order is the source timestamp order; downstream sliding-window deletions "
            "represent edge expiry rather than observed deletion events"
        ),
        "research_claim": False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
