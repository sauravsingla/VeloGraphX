#!/usr/bin/env python3
import argparse
import json
import math
from pathlib import Path
from statistics import mean, median


def percentile(xs, q):
    if not xs:
        return 0.0
    ys = sorted(xs)
    if len(ys) == 1:
        return ys[0]
    pos = (len(ys) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return ys[lo]
    w = pos - lo
    return ys[lo] * (1.0 - w) + ys[hi] * w


def parse_identity(path):
    parts = path.stem.split("__")
    dataset = parts[0]
    batch = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
    rep = int(parts[2]) if len(parts) > 2 and parts[2].isdigit() else 0
    return dataset, batch, rep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    paths = sorted(Path(args.input_dir).glob("*.json"))
    if not paths:
        raise SystemExit("no publication policy result files found")

    grouped = {}
    all_exact = True
    redundant_warmups = 0
    adaptive_initial_probes = 0

    for path in paths:
        d = json.loads(path.read_text())
        if d.get("artifact_type") != "velographx-publication-repair-recompute-policy":
            raise SystemExit(f"unexpected artifact type: {path}")
        all_exact = all_exact and bool(d.get("all_policies_exact"))
        dataset, batch, rep = parse_identity(path)
        oracle = d["oracle_batch_us"]
        oracle_full = d["oracle_full"]
        for p in d["policies"]:
            name = p["name"]
            key = (dataset, batch, name)
            row = grouped.setdefault(key, {
                "regrets": [], "latencies": [], "decision": [],
                "wrong": 0, "samples": 0, "fallbacks": 0,
                "fallback_us": 0.0, "full_choices": 0, "reps": set(),
            })
            if not p.get("exact", False):
                all_exact = False
            for i, (lat, ora) in enumerate(zip(p["batch_us"], oracle)):
                regret = max(0.0, (lat - ora) / max(1e-12, ora))
                row["regrets"].append(regret)
                row["latencies"].append(lat)
                row["decision"].append(p["decision_us"][i])
                chosen_full = bool(p["explicit_full"][i])
                row["full_choices"] += int(chosen_full)
                row["wrong"] += int(chosen_full != bool(oracle_full[i]))
                fallback = bool(p["internal_full_fallback"][i])
                row["fallbacks"] += int(fallback)
                row["fallback_us"] += float(p["fallback_total_us"][i])
                row["samples"] += 1
            row["reps"].add(rep)
            if name == "adaptive":
                for t in p.get("trace", []):
                    if t.get("reason") == "large_one_sided_full":
                        redundant_warmups += 1
                    if t.get("reason") == "large_initial_incremental_probe":
                        adaptive_initial_probes += 1

    rows = []
    for (dataset, batch, name), x in sorted(grouped.items()):
        regrets = x["regrets"]
        lat = x["latencies"]
        rows.append({
            "dataset": dataset,
            "batch_size": batch,
            "policy": name,
            "repetitions": len(x["reps"]),
            "samples": x["samples"],
            "mean_batch_us": mean(lat),
            "median_batch_us": median(lat),
            "p95_batch_us": percentile(lat, 0.95),
            "p99_batch_us": percentile(lat, 0.99),
            "mean_regret": mean(regrets),
            "median_regret": median(regrets),
            "p95_regret": percentile(regrets, 0.95),
            "p99_regret": percentile(regrets, 0.99),
            "max_regret": max(regrets),
            "wrong_arm_count": x["wrong"],
            "wrong_arm_rate": x["wrong"] / max(1, x["samples"]),
            "full_choice_fraction": x["full_choices"] / max(1, x["samples"]),
            "internal_fallback_count": x["fallbacks"],
            "internal_fallback_fraction": x["fallbacks"] / max(1, x["samples"]),
            "fallback_total_us": x["fallback_us"],
            "mean_decision_us": mean(x["decision"]),
        })

    adaptive = [r for r in rows if r["policy"] == "adaptive"]
    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-publication-policy-summary",
        "all_results_exact": all_exact,
        "adaptive_redundant_one_sided_full_count": redundant_warmups,
        "adaptive_initial_incremental_probe_count": adaptive_initial_probes,
        "adaptive_mean_regret_across_regimes": mean(r["mean_regret"] for r in adaptive),
        "adaptive_worst_regime_mean_regret": max(r["mean_regret"] for r in adaptive),
        "adaptive_worst_regime_p95_regret": max(r["p95_regret"] for r in adaptive),
        "adaptive_max_batch_regret": max(r["max_regret"] for r in adaptive),
        "rows": rows,
    }
    Path(args.output).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, sort_keys=True))

    if not all_exact:
        raise SystemExit("publication policy exactness failed")
    if redundant_warmups != 0:
        raise SystemExit("redundant large_one_sided_full decision reappeared")


if __name__ == "__main__":
    main()
