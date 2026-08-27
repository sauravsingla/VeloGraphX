#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import statistics
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def load_rows(path: Path):
    required = {"family", "codec", "values", "encoded_bytes", "ratio", "encode_us", "decode_us"}
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            missing = sorted(required - set(reader.fieldnames or []))
            raise ValueError(f"codec CSV missing columns: {', '.join(missing)}")
        rows = []
        for line, row in enumerate(reader, 2):
            try:
                rows.append({
                    "family": row["family"],
                    "codec": row["codec"],
                    "values": int(row["values"]),
                    "encoded_bytes": int(row["encoded_bytes"]),
                    "ratio": float(row["ratio"]),
                    "encode_us": float(row["encode_us"]),
                    "decode_us": float(row["decode_us"]),
                })
            except Exception as exc:
                raise ValueError(f"invalid codec CSV row {line}: {exc}") from exc
    return rows


def calibrate(rows):
    by_family = {}
    for row in rows:
        by_family.setdefault(row["family"], {})[row["codec"]] = row

    observations = []
    faster_overheads = []
    for family, codecs in sorted(by_family.items()):
        baseline = codecs.get("varbyte") or codecs.get("blocked-varbyte")
        vector = codecs.get("fixed-width-vectorized")
        if baseline is None or vector is None:
            continue
        if baseline["encoded_bytes"] <= 0 or baseline["decode_us"] <= 0:
            continue
        overhead = vector["encoded_bytes"] / baseline["encoded_bytes"]
        decode_speedup = baseline["decode_us"] / max(vector["decode_us"], 1e-12)
        faster = decode_speedup > 1.0
        if faster:
            faster_overheads.append(overhead)
        observations.append({
            "family": family,
            "baseline_codec": baseline["codec"],
            "fixed_codec": vector["codec"],
            "encoded_size_overhead": overhead,
            "decode_speedup": decode_speedup,
            "fixed_width_faster": faster,
        })

    if not observations:
        raise ValueError("no comparable varbyte/fixed-width-vectorized rows found")

    recommended = statistics.median(faster_overheads) if faster_overheads else 1.0
    return observations, recommended


def main():
    parser = argparse.ArgumentParser(description="Calibrate VeloGraphX fixed-width codec overhead from codec benchmark CSV.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    rows = load_rows(args.input)
    observations, recommended = calibrate(rows)
    report = {
        "schema_version": 1,
        "input": str(args.input),
        "input_sha256": sha256_file(args.input),
        "method": "median encoded-size overhead among families where vectorized fixed-width decode beats varbyte baseline",
        "recommended_max_fixed_width_overhead": recommended,
        "observation_count": len(observations),
        "observations": observations,
        "research_claim": False,
    }
    text = json.dumps(report, sort_keys=True, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
