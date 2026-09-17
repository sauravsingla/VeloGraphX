#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from statistics import mean


def percentile(values, q):
    if not values:
        return 0.0
    xs = sorted(values)
    if len(xs) == 1:
        return xs[0]
    pos = (len(xs) - 1) * q
    lo, hi = math.floor(pos), math.ceil(pos)
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
        raise SystemExit("no selector ablation results")

    grouped = {}
    all_exact = True
    for path in paths:
        d = json.loads(path.read_text())
        if d.get("artifact_type") != "velographx-publication-selector-feature-ablation":
            raise SystemExit(f"unexpected artifact type: {path}")
        all_exact = all_exact and bool(d.get("all_policies_exact"))
        dataset, batch, rep = identity(path)
        oracle = d["oracle_batch_us"]
        oracle_full = d["oracle_full"]
        for p in d["policies"]:
            all_exact = all_exact and bool(p.get("exact"))
            key = (dataset, batch, p["name"])
            x = grouped.setdefault(key, {
                "reps": set(), "regrets": [], "wrong": 0, "samples": 0,
                "decision_us": [], "full": 0,
            })
            x["reps"].add(rep)
            for i, (lat, ora) in enumerate(zip(p["batch_us"], oracle)):
                x["regrets"].append(max(0.0, (lat - ora) / max(1e-12, ora)))
                chosen_full = bool(p["explicit_full"][i])
                x["wrong"] += int(chosen_full != bool(oracle_full[i]))
                x["full"] += int(chosen_full)
                x["samples"] += 1
                x["decision_us"].append(float(p["decision_us"][i]))

    rows = []
    for (dataset, batch, policy), x in sorted(grouped.items()):
        rows.append({
            "dataset": dataset,
            "batch_size": batch,
            "policy": policy,
            "repetitions": len(x["reps"]),
            "samples": x["samples"],
            "mean_regret": mean(x["regrets"]),
            "p95_regret": percentile(x["regrets"], 0.95),
            "max_regret": max(x["regrets"]),
            "wrong_arm_rate": x["wrong"] / max(1, x["samples"]),
            "full_choice_fraction": x["full"] / max(1, x["samples"]),
            "mean_decision_us": mean(x["decision_us"]),
        })

    policies = sorted(set(r["policy"] for r in rows))
    aggregates = {}
    for policy in policies:
        subset = [r for r in rows if r["policy"] == policy]
        aggregates[policy] = {
            "regimes": len(subset),
            "equal_regime_mean_regret": mean(r["mean_regret"] for r in subset),
            "worst_regime_mean_regret": max(r["mean_regret"] for r in subset),
            "worst_regime_p95_regret": max(r["p95_regret"] for r in subset),
            "sample_weighted_wrong_arm_rate": sum(
                r["wrong_arm_rate"] * r["samples"] for r in subset
            ) / max(1, sum(r["samples"] for r in subset)),
        }

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-publication-selector-feature-ablation-summary",
        "all_results_exact": all_exact,
        "one_factor_at_a_time": {
            "adaptive_no_structural": "remove update-density/reachability/shallow-deletion preflight guards only",
            "adaptive_no_affected": "remove previous affected-fraction cost inflation only",
            "adaptive_no_uncertainty": "replace large-graph confidence-bound comparison by point-estimate comparison only",
        },
        "aggregates": aggregates,
        "rows": rows,
    }
    Path(args.output).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, sort_keys=True))
    if not all_exact:
        raise SystemExit("selector ablation exactness failed")


if __name__ == "__main__":
    main()
