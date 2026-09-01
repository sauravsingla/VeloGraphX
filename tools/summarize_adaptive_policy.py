#!/usr/bin/env python3
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path

POLICIES = ["always_incremental", "always_full", "simple_threshold", "adaptive"]


def mean(values):
    values = list(values)
    if not values:
        raise ValueError("cannot compute a mean from an empty sequence")
    return sum(values) / len(values)


def policy_regret(result, oracle_batch_us, path):
    if "regret_vs_batch_oracle" in result:
        return float(result["regret_vs_batch_oracle"])
    batch_us = result.get("batch_us")
    if not isinstance(batch_us, list) or not isinstance(oracle_batch_us, list):
        raise SystemExit(f"missing batch_us/oracle_batch_us needed for regret: {path}")
    if len(batch_us) != len(oracle_batch_us) or not batch_us:
        raise SystemExit(f"mismatched or empty batch/oracle timings in {path}")
    regrets = []
    for observed, oracle in zip(batch_us, oracle_batch_us):
        observed, oracle = float(observed), float(oracle)
        if oracle < 0.0 or observed < 0.0:
            raise SystemExit(f"negative timing in {path}")
        regret = 0.0 if oracle == 0.0 and observed == 0.0 else (math.inf if oracle == 0.0 else (observed-oracle)/oracle)
        if not math.isfinite(regret):
            raise SystemExit(f"non-finite oracle regret in {path}")
        regrets.append(max(0.0, regret))
    return mean(regrets)


def is_policy_record(d):
    if d.get("artifact_type") == "velographx-adaptive-policy-ablation":
        return True
    return int(d.get("schema_version", 0)) >= 6 and isinstance(d.get("policies"), list) and isinstance(d.get("oracle_batch_us"), list)


def parse_result_name(path):
    parts = path.stem.split("__")
    if len(parts) == 3:
        dataset, batch, rep = parts
        return dataset, int(batch), int(rep)
    if len(parts) == 4 and parts[1].startswith("r") and parts[2].startswith("b"):
        dataset = f"{parts[0]}-{parts[1]}"
        return dataset, int(parts[2][1:]), int(parts[3])
    raise ValueError(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", type=Path, required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    args = ap.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    grouped = defaultdict(list)
    inspected = 0
    for path in sorted(args.input_dir.glob("*.json")):
        inspected += 1
        d = json.loads(path.read_text())
        if not is_policy_record(d):
            continue
        try:
            dataset, batch, rep = parse_result_name(path)
        except ValueError as exc:
            raise SystemExit(f"invalid policy result filename: {path}") from exc
        if not d.get("all_policies_exact") or not all(p.get("exact") for p in d.get("policies", [])):
            raise SystemExit(f"inexact policy result: {path}")
        grouped[(dataset, batch)].append((rep, path, d))

    if not grouped:
        raise SystemExit(f"no adaptive-policy result artifacts found in {args.input_dir} after inspecting {inspected} JSON file(s)")

    repetition_counts = {len(reps) for reps in grouped.values()}
    if len(repetition_counts) != 1:
        raise SystemExit(f"inconsistent repetition counts: {sorted(repetition_counts)}")
    repetitions = next(iter(repetition_counts))
    if repetitions < 1:
        raise SystemExit("no repetitions")

    rows = []
    dataset_summary = defaultdict(list)
    expected_reps = list(range(1, repetitions + 1))
    for (dataset, batch), reps in sorted(grouped.items()):
        rep_ids = sorted(rep for rep, _, _ in reps)
        if rep_ids != expected_reps:
            raise SystemExit(f"expected repetitions {expected_reps} for {dataset}/{batch}, got {rep_ids}")
        metrics = {p: {"means": [], "regrets": [], "full": []} for p in POLICIES}
        for _, path, d in reps:
            by_name = {p.get("name"): p for p in d["policies"]}
            missing = [policy for policy in POLICIES if policy not in by_name]
            if missing:
                raise SystemExit(f"missing policies {missing} in {path}")
            oracle_batch_us = d.get("oracle_batch_us")
            for policy in POLICIES:
                p = by_name[policy]
                batch_us = p.get("batch_us")
                if "mean_batch_us" in p:
                    mean_batch_us = float(p["mean_batch_us"])
                elif isinstance(batch_us, list) and batch_us:
                    mean_batch_us = mean(float(x) for x in batch_us)
                else:
                    raise SystemExit(f"missing mean/batch latency for {policy} in {path}")
                metrics[policy]["means"].append(mean_batch_us)
                metrics[policy]["regrets"].append(policy_regret(p, oracle_batch_us, path))
                metrics[policy]["full"].append(float(p["full_recompute_batches"]))

        policy_means = {p: mean(metrics[p]["means"]) for p in POLICIES}
        best = min(policy_means, key=policy_means.get)
        for policy in POLICIES:
            mean_us = policy_means[policy]
            row = {
                "dataset": dataset,
                "batch_size": batch,
                "policy": policy,
                "mean_batch_us": mean_us,
                "mean_regret_vs_batch_oracle": mean(metrics[policy]["regrets"]),
                "mean_full_recompute_batches": mean(metrics[policy]["full"]),
                "fastest_policy_for_regime": best,
                "relative_to_regime_best": mean_us / policy_means[best],
            }
            rows.append(row)
            dataset_summary[dataset].append(row)

    csv_path = args.output_dir / "policy-regime-summary.csv"
    with csv_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader(); writer.writerows(rows)

    summary = {
        "schema_version": 3,
        "artifact_type": "velographx-adaptive-policy-crossover-summary",
        "input_schema_compatibility": ["historical-tagged", "schema-v6", "rooted-schema-v6"],
        "regret_definition": "mean per-batch max(0, (policy_us - oracle_us) / oracle_us)",
        "repetitions_per_regime": repetitions,
        "all_results_exact": True,
        "rows": rows,
        "datasets": {},
    }
    for dataset, dsrows in dataset_summary.items():
        adaptive = [r for r in dsrows if r["policy"] == "adaptive"]
        wins = sum(r["fastest_policy_for_regime"] == "adaptive" for r in adaptive)
        summary["datasets"][dataset] = {
            "regimes": len(adaptive),
            "adaptive_regime_wins": wins,
            "adaptive_mean_regret": mean(r["mean_regret_vs_batch_oracle"] for r in adaptive),
            "adaptive_mean_relative_to_regime_best": mean(r["relative_to_regime_best"] for r in adaptive),
            "crossover": [{"batch_size": r["batch_size"], "fastest_policy": r["fastest_policy_for_regime"]} for r in adaptive],
        }
    (args.output_dir / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
