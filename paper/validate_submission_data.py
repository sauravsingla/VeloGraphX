#!/usr/bin/env python3
"""Validate committed manuscript evidence inputs before submission.

This script uses only the Python standard library. It guards against accidental drift
between the audited selector summary and the paper-facing CSV/JSON inputs.
"""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
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
