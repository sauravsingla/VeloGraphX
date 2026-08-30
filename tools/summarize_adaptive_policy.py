#!/usr/bin/env python3
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path

POLICIES = ["always_incremental", "always_full", "simple_threshold", "adaptive"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", type=Path, required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    args = ap.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    grouped = defaultdict(list)
    for path in sorted(args.input_dir.glob("*.json")):
        d = json.loads(path.read_text())
        if d.get("artifact_type") != "velographx-adaptive-policy-ablation":
            continue
        parts = path.stem.split("__")
        if len(parts) < 3:
            continue
        dataset, batch, rep = parts[0], int(parts[1]), int(parts[2])
        if not d.get("all_policies_exact"):
            raise SystemExit(f"inexact policy result: {path}")
        grouped[(dataset, batch)].append((rep, d))

    rows = []
    dataset_summary = defaultdict(list)
    for (dataset, batch), reps in sorted(grouped.items()):
        if len(reps) != 5:
            raise SystemExit(f"expected five repetitions for {dataset}/{batch}, got {len(reps)}")
        metrics = {p: {"means": [], "regrets": [], "full": []} for p in POLICIES}
        for _, d in reps:
            by_name = {p["name"]: p for p in d["policies"]}
            for policy in POLICIES:
                p = by_name[policy]
                metrics[policy]["means"].append(p["mean_batch_us"])
                metrics[policy]["regrets"].append(p["regret_vs_batch_oracle"])
                metrics[policy]["full"].append(p["full_recompute_batches"])
        policy_means = {p: sum(metrics[p]["means"]) / 5 for p in POLICIES}
        best = min(policy_means, key=policy_means.get)
        for policy in POLICIES:
            mean_us = policy_means[policy]
            regret = sum(metrics[policy]["regrets"]) / 5
            full = sum(metrics[policy]["full"]) / 5
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
        writer.writeheader(); writer.writerows(rows)

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-adaptive-policy-crossover-summary",
        "repetitions_per_regime": 5,
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
            "adaptive_mean_regret": sum(r["mean_regret_vs_batch_oracle"] for r in adaptive) / len(adaptive),
            "adaptive_mean_relative_to_regime_best": sum(r["relative_to_regime_best"] for r in adaptive) / len(adaptive),
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
