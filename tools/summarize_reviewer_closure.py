#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path


def percentile(values: list[float], q: float) -> float:
    if not values:
        return 0.0
    data = sorted(values)
    if len(data) == 1:
        return data[0]
    pos = (len(data) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return data[lo]
    w = pos - lo
    return data[lo] * (1.0 - w) + data[hi] * w


def load_jsons(path: Path) -> list[tuple[Path, dict]]:
    return [(p, json.loads(p.read_text())) for p in sorted(path.glob("*.json"))]


def summarize_policy_runs(items: list[tuple[Path, dict]], policy_name: str = "adaptive") -> dict:
    regrets: list[float] = []
    decisions: list[float] = []
    wrong = 0
    total = 0
    full_choices = 0
    fallbacks = 0
    regimes = []
    all_exact = True

    for path, d in items:
        all_exact = all_exact and bool(d.get("all_policies_exact", False))
        policies = {p["name"]: p for p in d["policies"]}
        target = policies[policy_name]
        inc = policies["always_incremental"]["batch_us"]
        full = policies["always_full"]["batch_us"]
        selected = target["batch_us"]
        explicit_full = target.get("explicit_full", [False] * len(selected))
        internal = target.get("internal_full_fallback", [False] * len(selected))
        decision_us = target.get("decision_us", [])
        decisions.extend(decision_us)

        local_regrets = []
        local_wrong = 0
        for i, value in enumerate(selected):
            oracle_full = not (inc[i] < full[i])  # full wins ties
            oracle = full[i] if oracle_full else inc[i]
            regret = max(0.0, (value - oracle) / oracle) if oracle > 0 else 0.0
            regrets.append(regret)
            local_regrets.append(regret)
            is_wrong = bool(explicit_full[i]) != oracle_full
            wrong += int(is_wrong)
            local_wrong += int(is_wrong)
            total += 1
            full_choices += int(bool(explicit_full[i]))
            fallbacks += int(bool(internal[i]))

        regimes.append({
            "file": path.name,
            "samples": len(selected),
            "mean_regret": statistics.mean(local_regrets) if local_regrets else 0.0,
            "p95_regret": percentile(local_regrets, 0.95),
            "max_regret": max(local_regrets, default=0.0),
            "wrong_arm_rate": local_wrong / len(selected) if selected else 0.0,
        })

    return {
        "all_exact": all_exact,
        "files": len(items),
        "samples": total,
        "mean_regret": statistics.mean(regrets) if regrets else 0.0,
        "median_regret": percentile(regrets, 0.50),
        "p95_regret": percentile(regrets, 0.95),
        "p99_regret": percentile(regrets, 0.99),
        "max_regret": max(regrets, default=0.0),
        "wrong_arm_rate": wrong / total if total else 0.0,
        "full_choice_fraction": full_choices / total if total else 0.0,
        "internal_fallback_fraction": fallbacks / total if total else 0.0,
        "mean_decision_us": statistics.mean(decisions) if decisions else 0.0,
        "regimes": regimes,
    }


def summarize_ablation(items: list[tuple[Path, dict]]) -> dict:
    all_exact = True
    rows = []
    for path, d in items:
        all_exact = all_exact and bool(d.get("all_stages_exact", False))
        for stage in d["stages"]:
            rows.append({
                "file": path.name,
                "stage": stage["stage"],
                "mechanism": stage["mechanism"],
                "exact": stage["exact"],
                "mean_batch_us": stage["mean_batch_us"],
                "mean_oracle_regret": stage["mean_oracle_regret"],
                "max_oracle_regret": stage["max_oracle_regret"],
                "full_recompute_batches": stage["full_recompute_batches"],
                "internal_fallback_batches": stage["internal_fallback_batches"],
                "fallback_execution_us": stage["fallback_execution_us"],
            })
    by_stage = {}
    for name in [f"A{i}" for i in range(8)]:
        group = [r for r in rows if r["stage"] == name]
        if not group:
            continue
        by_stage[name] = {
            "mechanism": group[0]["mechanism"],
            "runs": len(group),
            "mean_batch_us": statistics.mean(r["mean_batch_us"] for r in group),
            "mean_oracle_regret": statistics.mean(r["mean_oracle_regret"] for r in group),
            "worst_max_oracle_regret": max(r["max_oracle_regret"] for r in group),
            "internal_fallback_batches": sum(r["internal_fallback_batches"] for r in group),
            "fallback_execution_us": sum(r["fallback_execution_us"] for r in group),
            "all_exact": all(r["exact"] for r in group),
        }
    return {"all_exact": all_exact, "files": len(items), "stages": by_stage, "rows": rows}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--heldout-dir", type=Path, required=True)
    parser.add_argument("--ablation-dir", type=Path, required=True)
    parser.add_argument("--triangle-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    heldout = summarize_policy_runs(load_jsons(args.heldout_dir))
    triangles = summarize_policy_runs(load_jsons(args.triangle_dir))
    ablation = summarize_ablation(load_jsons(args.ablation_dir))

    summary = {
        "schema_version": 1,
        "artifact_type": "velographx-reviewer-closure-summary",
        "research_claim": False,
        "hosted_runner_performance_gate": False,
        "heldout_bfs": heldout,
        "cross_algorithm_triangles": triangles,
        "clean_ablation": ablation,
        "all_exact": heldout["all_exact"] and triangles["all_exact"] and ablation["all_exact"],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if summary["all_exact"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
