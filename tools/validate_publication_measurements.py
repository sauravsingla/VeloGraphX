#!/usr/bin/env python3
"""Quality gate and summarize raw publication benchmark samples."""

import argparse
import json
import statistics
from pathlib import Path


def summary(values):
    ordered = sorted(values)
    return {
        "count": len(values), "raw_samples_us": values,
        "median_us": statistics.median(values),
        "mean_us": statistics.mean(values),
        "stdev_us": statistics.stdev(values) if len(values) > 1 else 0.0,
        "min_us": ordered[0], "max_us": ordered[-1],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True,
                        help="JSON object mapping thread counts to microsecond sample arrays")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--minimum-samples", type=int, default=5)
    parser.add_argument("--noise-floor-us", type=float, default=1000.0)
    parser.add_argument("--require-scaling", action="store_true")
    args = parser.parse_args()
    raw = json.loads(args.input.read_text())
    if not isinstance(raw, dict) or not raw:
        raise ValueError("input must be a non-empty object keyed by thread count")
    groups, flags = {}, []
    for key, values in raw.items():
        threads = int(key)
        if threads < 1 or not isinstance(values, list) or len(values) < args.minimum_samples:
            raise ValueError(f"thread group {key} has too few samples")
        numeric = [float(value) for value in values]
        if any(value <= 0 for value in numeric):
            raise ValueError(f"thread group {key} contains a non-positive sample")
        groups[str(threads)] = summary(numeric)
        if groups[str(threads)]["median_us"] < args.noise_floor_us:
            flags.append(f"threads={threads}: median below noise floor")
    ordered = sorted((int(k), v["median_us"]) for k, v in groups.items())
    distinct = len({round(value, 9) for _, value in ordered}) > 1
    if args.require_scaling and (len(ordered) < 2 or not distinct):
        raise ValueError("thread scaling gate requires distinct measurements for at least two thread counts")
    result = {"schema_version": 1, "artifact_type": "publication-measurement-quality",
              "noise_floor_us": args.noise_floor_us, "groups": groups, "quality_flags": flags,
              "all_medians_above_noise_floor": not flags, "thread_scaling_observed": distinct,
              "research_claim": False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    if flags:
        print("; ".join(flags), file=__import__("sys").stderr)
        return 3
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
