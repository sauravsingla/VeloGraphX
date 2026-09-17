#!/usr/bin/env python3
"""Summarize deployment-style 0.35 fallback replay evidence.

The fallback-only path is the production IncrementalBFS behavior without a
pre-repair selector. The selector+fallback path replays the frozen publication
selector decisions and retains the same production fallback as a safety net.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from statistics import mean, median


def percentile(values, q):
    if not values:
        return 0.0
    xs = sorted(values)
    if len(xs) == 1:
        return xs[0]
    pos = (len(xs) - 1) * q
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return xs[lo]
    w = pos - lo
    return xs[lo] * (1.0 - w) + xs[hi] * w


def identity(path: Path):
    parts = path.stem.split("__")
    return parts[0], int(parts[1]), int(parts[2])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    paths = sorted(Path(args.input_dir).glob("*.json"))
    if not paths:
        raise SystemExit("no fallback replay JSON files found")

    grouped = {}
    global_samples = []
    all_exact = True

    for path in paths:
        data = json.loads(path.read_text())
        if data.get("artifact_type") != "velographx-production-fallback-replay":
            raise SystemExit(f"unexpected artifact type: {path}")
        all_exact = all_exact and bool(data.get("all_policies_exact"))
        dataset, batch, rep = identity(path)
        policies = {p["name"]: p for p in data["policies"]}
        required = {"fallback_only", "selector_plus_fallback", "always_full"}
        if set(policies) != required:
            raise SystemExit(f"unexpected policies in {path}: {set(policies)}")
        fb = policies["fallback_only"]
        sel = policies["selector_plus_fallback"]
        full = policies["always_full"]
        for p in policies.values():
            all_exact = all_exact and bool(p.get("exact"))
        if not (len(fb["batch_us"]) == len(sel["batch_us"]) == len(full["batch_us"])):
            raise SystemExit(f"batch length mismatch in {path}")

        key = (dataset, batch)
        row = grouped.setdefault(key, {
            "reps": set(), "samples": 0, "fallbacks": 0, "avoided": 0,
            "remaining": 0, "explicit_full": 0, "false_full": 0,
            "fallback_double_work_us": [], "avoided_double_work_us": [],
            "selector_savings_us": [], "false_full_penalty_us": [],
            "fallback_only_us": [], "selector_us": [], "full_us": [],
        })
        row["reps"].add(rep)

        for i, fb_us in enumerate(fb["batch_us"]):
            sel_us = float(sel["batch_us"][i])
            full_us = float(full["batch_us"][i])
            fallback = bool(fb["internal_full_fallback"][i])
            explicit = bool(sel["explicit_full"][i])
            sel_fallback = bool(sel["internal_full_fallback"][i])
            double_work = max(0.0, float(fb_us) - full_us) if fallback else 0.0
            avoided = fallback and explicit
            false_full = explicit and not fallback
            sample = {
                "dataset": dataset,
                "batch_size": batch,
                "rep": rep,
                "batch_index": i,
                "fallback_only_us": float(fb_us),
                "selector_plus_fallback_us": sel_us,
                "always_full_us": full_us,
                "fallback_only_internal_fallback": fallback,
                "selector_explicit_full": explicit,
                "selector_internal_fallback": sel_fallback,
                "observed_fallback_double_work_us": double_work,
                "selector_savings_vs_fallback_only_us": float(fb_us) - sel_us,
                "avoided_fallback_opportunity": avoided,
                "false_full": false_full,
            }
            global_samples.append(sample)
            row["samples"] += 1
            row["fallbacks"] += int(fallback)
            row["avoided"] += int(avoided)
            row["remaining"] += int(sel_fallback)
            row["explicit_full"] += int(explicit)
            row["false_full"] += int(false_full)
            row["fallback_only_us"].append(float(fb_us))
            row["selector_us"].append(sel_us)
            row["full_us"].append(full_us)
            row["selector_savings_us"].append(float(fb_us) - sel_us)
            if fallback:
                row["fallback_double_work_us"].append(double_work)
            if avoided:
                row["avoided_double_work_us"].append(double_work)
            if false_full:
                row["false_full_penalty_us"].append(max(0.0, sel_us - float(fb_us)))

    rows = []
    for (dataset, batch), x in sorted(grouped.items()):
        rows.append({
            "dataset": dataset,
            "batch_size": batch,
            "repetitions": len(x["reps"]),
            "samples": x["samples"],
            "fallback_only_internal_fallback_count": x["fallbacks"],
            "fallback_only_internal_fallback_rate": x["fallbacks"] / max(1, x["samples"]),
            "selector_explicit_full_count": x["explicit_full"],
            "selector_remaining_internal_fallback_count": x["remaining"],
            "avoided_fallback_opportunities": x["avoided"],
            "avoided_fraction_of_fallback_opportunities": x["avoided"] / max(1, x["fallbacks"]),
            "false_full_count": x["false_full"],
            "observed_fallback_double_work_us": sum(x["fallback_double_work_us"]),
            "observed_avoided_double_work_us": sum(x["avoided_double_work_us"]),
            "mean_selector_savings_vs_fallback_only_us": mean(x["selector_savings_us"]),
            "median_fallback_only_us": median(x["fallback_only_us"]),
            "median_selector_plus_fallback_us": median(x["selector_us"]),
            "median_always_full_us": median(x["full_us"]),
            "p95_selector_savings_vs_fallback_only_us": percentile(x["selector_savings_us"], 0.95),
            "false_full_penalty_us": sum(x["false_full_penalty_us"]),
        })

    fallback_samples = [s for s in global_samples if s["fallback_only_internal_fallback"]]
    avoided_samples = [s for s in fallback_samples if s["avoided_fallback_opportunity"]]
    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-production-fallback-summary",
        "all_results_exact": all_exact,
        "samples": len(global_samples),
        "fallback_only_internal_fallback_count": len(fallback_samples),
        "selector_avoided_fallback_opportunities": len(avoided_samples),
        "selector_avoided_fraction_of_fallback_opportunities": (
            len(avoided_samples) / max(1, len(fallback_samples))
        ),
        "selector_remaining_internal_fallback_count": sum(
            int(s["selector_internal_fallback"]) for s in global_samples
        ),
        "selector_false_full_count": sum(int(s["false_full"]) for s in global_samples),
        "observed_fallback_double_work_us": sum(
            s["observed_fallback_double_work_us"] for s in fallback_samples
        ),
        "observed_avoided_double_work_us": sum(
            s["observed_fallback_double_work_us"] for s in avoided_samples
        ),
        "mean_selector_savings_vs_fallback_only_us": mean(
            s["selector_savings_vs_fallback_only_us"] for s in global_samples
        ),
        "mean_selector_savings_on_fallback_batches_us": (
            mean(s["selector_savings_vs_fallback_only_us"] for s in fallback_samples)
            if fallback_samples else 0.0
        ),
        "method_note": (
            "Observed fallback double work is conservatively estimated as max(0, "
            "fallback-only answer-ready latency minus the separately measured full-recompute "
            "latency for the aligned batch). Selector decisions are replayed from the frozen "
            "publication-preflight-v1 trace; production fallback remains 0.35."
        ),
        "rows": rows,
    }
    Path(args.output).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, sort_keys=True))
    if not all_exact:
        raise SystemExit("fallback replay exactness failed")


if __name__ == "__main__":
    main()
