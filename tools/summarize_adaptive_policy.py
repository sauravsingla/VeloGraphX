#!/usr/bin/env python3
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path

POLICIES = ["always_incremental", "always_full", "simple_threshold", "adaptive"]


def mean(values):
    if not values:
        raise ValueError("cannot compute a mean from an empty sequence")
    return sum(values) / len(values)


def policy_regret(result, oracle_batch_us, path):
    """Return mean per-batch oracle-relative regret for old and schema-v6 records."""
    if "regret_vs_batch_oracle" in result:
        return float(result["regret_vs_batch_oracle"])

    batch_us = result.get("batch_us")
    if not isinstance(batch_us, list) or not isinstance(oracle_batch_us, list):
        raise SystemExit(f"missing batch_us/oracle_batch_us needed for regret: {path}")
    if len(batch_us) != len(oracle_batch_us) or not batch_us:
        raise SystemExit(
            f"mismatched or empty batch/oracle timings in {path}: "
            f"{len(batch_us)} vs {len(oracle_batch_us)}"
        )

    regrets = []
    for observed, oracle in zip(batch_us, oracle_batch_us):
        observed = float(observed)
        oracle = float(oracle)
        if oracle < 0.0 or observed < 0.0:
            raise SystemExit(f"negative timing in {path}")
        if oracle == 0.0:
            regret = 0.0 if observed == 0.0 else math.inf
        else:
            regret = (observed - oracle) / oracle
        if not math.isfinite(regret):
            raise SystemExit(f"non-finite oracle regret in {path}")
        # The oracle is the minimum observed policy latency for each batch. Tiny
        # negative values can only arise from floating-point roundoff.
        regrets.append(max(0.0, regret))
    return mean(regrets)


def is_policy_record(d):
    """Accept the historical tagged artifact and the current schema-v6 harness."""
    if d.get("artifact_type") == "velographx-adaptive-policy-ablation":
        return True
    return (
        int(d.get("schema_version", 0)) >= 6
        and isinstance(d.get("policies"), list)
        and isinstance(d.get("oracle_batch_us"), list)
    )


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
        parts = path.stem.split("__")
        if len(parts) < 3:
            continue
        try:
            dataset, batch, rep = parts[0], int(parts[1]), int(parts[2])
        except ValueError as exc:
            raise SystemExit(f"invalid policy result filename: {path}") from exc
        if not d.get("all_policies_exact"):
            raise SystemExit(f"inexact policy result: {path}")
        if not all(p.get("exact") for p in d.get("policies", [])):
            raise SystemExit(f"inexact per-policy result: {path}")
        grouped[(dataset, batch)].append((rep, path, d))

    if not grouped:
        raise SystemExit(
            f"no adaptive-policy result artifacts found in {args.input_dir} "
            f"after inspecting {inspected} JSON file(s)"
        )

    rows = []
    dataset_summary = defaultdict(list)
    for (dataset, batch), reps in sorted(grouped.items()):
        if len(reps) != 5:
            raise SystemExit(f"expected five repetitions for {dataset}/{batch}, got {len(reps)}")
        rep_ids = sorted(rep for rep, _, _ in reps)
        if rep_ids != [1, 2, 3, 4, 5]:
            raise SystemExit(f"expected repetitions 1..5 for {dataset}/{batch}, got {rep_ids}")

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
            regret = mean(metrics[policy]["regrets"])
            full = mean(metrics[policy]["full"])
            row = {
                "dataset": dataset,
                "batch_size": batch,
                "policy": policy,
                "mean_batch_us": mean_us,
                "mean_regret_vs_batch_oracle": regret,
                "mean_full_recompute_batches": full,
                "fastest_policy_for_regime": best,
                "relative_to_regime_best": mean_us / policy_means[best],
            }
            rows.append(row)
            dataset_summary[dataset].append(row)

    csv_path = args.output_dir / "policy-regime-summary.csv"
    with csv_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "schema_version": 2,
        "artifact_type": "velographx-adaptive-policy-crossover-summary",
        "input_schema_compatibility": ["historical-tagged", "schema-v6"],
        "regret_definition": "mean per-batch max(0, (policy_us - oracle_us) / oracle_us)",
        "repetitions_per_regime": 5,
        "all_results_exact": True,
        "rows": rows,
        "datasets": {},
    }
    for dataset, dsrows in dataset_summary.items():
        adaptive = [r for r in dsrows if r["policy"] == "adaptive"]
        if not adaptive:
            raise SystemExit(f"no adaptive rows summarized for {dataset}")
        wins = sum(r["fastest_policy_for_regime"] == "adaptive" for r in adaptive)
        summary["datasets"][dataset] = {
            "regimes": len(adaptive),
            "adaptive_regime_wins": wins,
            "adaptive_mean_regret": mean(r["mean_regret_vs_batch_oracle"] for r in adaptive),
            "adaptive_mean_relative_to_regime_best": mean(r["relative_to_regime_best"] for r in adaptive),
            "crossover": [
                {"batch_size": r["batch_size"], "fastest_policy": r["fastest_policy_for_regime"]}
                for r in adaptive
            ],
        }
    (args.output_dir / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")

    # Matplotlib is installed by the workflow. Two publication-oriented figures.
    import matplotlib.pyplot as plt
    for dataset in sorted(dataset_summary):
        dsrows = dataset_summary[dataset]
        fig = plt.figure(figsize=(7.2, 4.6))
        ax = fig.add_subplot(111)
        for policy in POLICIES:
            rr = sorted((r for r in dsrows if r["policy"] == policy), key=lambda x: x["batch_size"])
            ax.plot([r["batch_size"] for r in rr], [r["mean_batch_us"] for r in rr], marker="o", label=policy)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("Batch size (edge additions; same number of deletions)")
        ax.set_ylabel("Mean answer-ready batch latency (us)")
        ax.set_title(f"Policy crossover: {dataset}")
        ax.grid(True, alpha=0.25)
        ax.legend()
        fig.tight_layout()
        fig.savefig(args.output_dir / f"{dataset}-crossover.png", dpi=180)
        plt.close(fig)

        fig = plt.figure(figsize=(7.2, 4.6))
        ax = fig.add_subplot(111)
        for policy in POLICIES:
            rr = sorted((r for r in dsrows if r["policy"] == policy), key=lambda x: x["batch_size"])
            ax.plot([r["batch_size"] for r in rr], [100.0 * r["mean_regret_vs_batch_oracle"] for r in rr], marker="o", label=policy)
        ax.set_xscale("log", base=2)
        ax.set_xlabel("Batch size (edge additions; same number of deletions)")
        ax.set_ylabel("Regret vs per-batch oracle (%)")
        ax.set_title(f"Policy regret: {dataset}")
        ax.grid(True, alpha=0.25)
        ax.legend()
        fig.tight_layout()
        fig.savefig(args.output_dir / f"{dataset}-regret.png", dpi=180)
        plt.close(fig)

    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
