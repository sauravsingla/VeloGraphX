#!/usr/bin/env python3
"""Validate committed manuscript evidence inputs before submission.

This script uses only the Python standard library. It guards against accidental drift
between audited selector summaries, paper-facing CSV/JSON inputs, and the benchmark
workflow contract used to produce the retained evidence.
"""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
DATA = ROOT / "data"


def close(a: float, b: float, tol: float = 1e-12) -> bool:
    return math.isclose(a, b, rel_tol=tol, abs_tol=tol)


def fail(message: str) -> None:
    raise SystemExit(f"submission-data validation failed: {message}")


def main() -> None:
    results = json.loads((DATA / "accepted-results.json").read_text())
    if results.get("schema_version") != 2:
        fail("unexpected accepted-results schema version")

    policy = results.get("policy", {})
    if policy.get("cross_run_absolute_ranking") is not False:
        fail("cross-run absolute ranking must remain disabled")
    if policy.get("dynamic_exactness_required") is not True:
        fail("dynamic exactness must remain mandatory")
    if policy.get("retain_negative_results") is not True:
        fail("negative-result retention must remain enabled")

    selector = results["adaptive_selector"]["cross_dataset_validation"]
    if selector["run_id"] != 34929398888 or selector["artifact_id"] != 10381490811:
        fail("current selector provenance changed without registry update")
    if selector["all_exact"] is not True:
        fail("current selector evidence is not exact")
    if selector["regimes"] != 9 or selector["repetitions_per_regime"] != 5:
        fail("unexpected current-selector regime/repetition contract")
    if selector["adaptive_batch_samples"] != 1610:
        fail("unexpected current-selector sample count")
    if selector["internal_fallback_count"] != 0:
        fail("current-selector internal fallback count changed")

    with (DATA / "current-selector-regimes.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))

    if len(rows) != 9:
        fail(f"expected 9 selector-regime rows, found {len(rows)}")

    expected_datasets = {"ca-GrQc", "soc-Epinions1", "web-Google"}
    if {row["dataset"] for row in rows} != expected_datasets:
        fail("selector CSV dataset set changed")

    total_samples = sum(int(row["samples"]) for row in rows)
    if total_samples != selector["adaptive_batch_samples"]:
        fail("selector CSV sample count disagrees with accepted-results.json")

    weighted_regret = sum(float(row["mean_regret"]) * int(row["samples"]) for row in rows) / total_samples
    weighted_wrong_arm = sum(float(row["wrong_arm_rate"]) * int(row["samples"]) for row in rows) / total_samples
    weighted_decision = sum(float(row["mean_decision_us"]) * int(row["samples"]) for row in rows) / total_samples
    mean_regime_regret = sum(float(row["mean_regret"]) for row in rows) / len(rows)
    worst_mean = max(float(row["mean_regret"]) for row in rows)
    worst_p95 = max(float(row["p95_regret"]) for row in rows)

    checks = {
        "sample-weighted regret": (weighted_regret, selector["sample_weighted_mean_oracle_regret"]),
        "sample-weighted wrong-arm rate": (weighted_wrong_arm, selector["sample_weighted_wrong_arm_rate"]),
        "sample-weighted decision cost": (weighted_decision, selector["sample_weighted_mean_decision_us"]),
        "mean regime regret": (mean_regime_regret, selector["mean_oracle_regret_across_regimes"]),
        "worst regime mean regret": (worst_mean, selector["worst_regime_mean_regret"]),
        "worst regime p95 regret": (worst_p95, selector["worst_regime_p95_regret"]),
    }
    for name, (actual, expected) in checks.items():
        if not close(actual, expected, tol=1e-10):
            fail(f"{name} mismatch: CSV={actual} registry={expected}")

    for row in rows:
        if int(row["repetitions"]) != 5:
            fail(f"unexpected repetition count in {row['dataset']} batch {row['batch_size']}")
        if float(row["internal_fallback_fraction"]) != 0.0:
            fail(f"nonzero internal fallback in {row['dataset']} batch {row['batch_size']}")

    # Reviewer-facing derived values are recomputed from retained artifacts and carry
    # the source run/artifact/digest explicitly. They must never float free of that provenance.
    derived = json.loads((DATA / "final-review-derived.json").read_text())
    if derived.get("schema_version") != 1:
        fail("unexpected final-review-derived schema version")

    plans = derived["selector_plan_comparison"]
    if plans["source_run_id"] != selector["run_id"] or plans["source_artifact_id"] != selector["artifact_id"]:
        fail("selector plan-comparison provenance disagrees with registry")
    if plans["source_artifact_sha256"] != selector["artifact_sha256"]:
        fail("selector plan-comparison digest disagrees with registry")
    plan_rows = plans["rows"]
    if len(plan_rows) != 9 or sum(int(r["samples"]) for r in plan_rows) != 1610:
        fail("selector plan-comparison row/sample contract changed")
    if {(r["dataset"], int(r["batch_size"])) for r in plan_rows} != {
        (r["dataset"], int(r["batch_size"])) for r in rows
    }:
        fail("selector plan-comparison regimes disagree with selector CSV")
    if any(float(r["median_incremental_over_full_ratio"]) <= 0.0 for r in plan_rows):
        fail("non-positive incremental/full crossover ratio")
    full_favored = {
        (r["dataset"], int(r["batch_size"]))
        for r in plan_rows
        if float(r["median_incremental_over_full_ratio"]) > 1.0
    }
    if full_favored != {("ca-GrQc", 1536), ("web-Google", 24576)}:
        fail(f"unexpected median crossover regimes: {sorted(full_favored)}")

    aggregates = plans["policy_aggregates"]
    expected_policies = {
        "always_incremental", "always_full", "simple_threshold", "history_cost_model", "adaptive"
    }
    if set(aggregates) != expected_policies:
        fail("policy aggregate set changed")
    adaptive = aggregates["adaptive"]
    if not close(adaptive["equal_regime_mean_regret"], selector["mean_oracle_regret_across_regimes"], 1e-10):
        fail("derived adaptive equal-regime regret disagrees with registry")
    if not close(adaptive["sample_weighted_mean_regret"], selector["sample_weighted_mean_oracle_regret"], 1e-10):
        fail("derived adaptive weighted regret disagrees with registry")
    if not close(adaptive["sample_weighted_wrong_arm_rate"], selector["sample_weighted_wrong_arm_rate"], 1e-10):
        fail("derived adaptive wrong-arm rate disagrees with registry")
    if not close(adaptive["worst_regime_mean_regret"], selector["worst_regime_mean_regret"], 1e-10):
        fail("derived adaptive worst-regime regret disagrees with registry")
    if not all(adaptive["equal_regime_mean_regret"] < aggregates[p]["equal_regime_mean_regret"] for p in expected_policies - {"adaptive"}):
        fail("adaptive policy is no longer best on the declared equal-regime robustness summary")

    nk_registry = results["external_baselines"]["networkit_dynamic_bfs"]
    nk = derived["networkit_dispersion"]
    if nk["source_run_id"] != nk_registry["run_id"] or nk["source_artifact_id"] != nk_registry["artifact_id"]:
        fail("NetworKit dispersion provenance disagrees with registry")
    if nk["source_artifact_sha256"] != nk_registry["artifact_sha256"]:
        fail("NetworKit dispersion digest disagrees with registry")
    for dataset, entry in nk["datasets"].items():
        mean = nk_registry["datasets"][dataset]["velographx_over_networkit_latency_ratio"]
        if not (entry["min_root_mean_ratio"] <= mean <= entry["max_root_mean_ratio"]):
            fail(f"NetworKit root range does not bracket dataset mean for {dataset}")

    # Keep the paper-facing workload table tied to the exact frozen workflow contract.
    workflow = (REPO / ".github" / "workflows" / "publication-selector-cross-dataset.yml").read_text()
    if "'soc-Epinions1': ('soc-Epinions1', 71391, 0.90, [384, 1536, 6144])" not in workflow:
        fail("soc-Epinions1 audited import fraction is no longer 0.90 in the selector workflow")

    if results["external_baselines"]["networkit_dynamic_bfs"]["all_exact"] is not True:
        fail("NetworKit paired evidence lost exactness flag")
    if results["exact_triangles_published_reference"]["all_exact"] is not True:
        fail("published triangle reference evidence lost exactness flag")
    if results["orkut_canonicalization_ab"]["all_correct"] is not True:
        fail("Orkut canonicalization A/B lost correctness flag")
    if results["fresh_multi_dataset_triangle_crossover"]["all_exact"] is not True:
        fail("multi-dataset triangle crossover lost exactness flag")

    print("submission-data validation passed")


if __name__ == "__main__":
    main()
