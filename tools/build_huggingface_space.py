#!/usr/bin/env python3
"""Prepare and validate the VeloGraphX Hugging Face Space.

The Space is a presentation layer only. Its data file is copied byte-for-byte
from the canonical publication table in paper/data so the dashboard cannot
silently drift from the retained research artifact.
"""

from __future__ import annotations

import csv
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "paper" / "data" / "current-selector-regimes.csv"
SPACE = ROOT / "hf-space"
TARGET = SPACE / "data" / "current-selector-regimes.csv"
INDEX = SPACE / "index.html"


def main() -> None:
    if not SOURCE.is_file():
        raise FileNotFoundError(SOURCE)
    if not INDEX.is_file():
        raise FileNotFoundError(INDEX)

    TARGET.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SOURCE, TARGET)

    with SOURCE.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise SystemExit("canonical selector-regimes CSV is empty")

    required = {
        "dataset",
        "batch_size",
        "samples",
        "median_batch_us",
        "mean_regret",
        "p95_regret",
        "max_regret",
        "wrong_arm_rate",
        "full_choice_fraction",
        "mean_decision_us",
    }
    if not required.issubset(rows[0]):
        missing = sorted(required.difference(rows[0]))
        raise SystemExit(f"canonical selector-regimes CSV is missing columns: {missing}")

    regime_count = len(rows)
    sample_count = sum(int(row["samples"]) for row in rows)
    datasets = sorted({row["dataset"] for row in rows})
    max_mean_regret = max(float(row["mean_regret"]) for row in rows)

    page = INDEX.read_text(encoding="utf-8")
    expected_static_values = {
        str(regime_count),
        f"{sample_count:,}",
        str(len(datasets)),
        f"{max_mean_regret:.4f}",
    }
    missing_values = sorted(value for value in expected_static_values if value not in page)
    if missing_values:
        raise SystemExit(
            "Space headline values are stale relative to canonical CSV: "
            + ", ".join(missing_values)
        )

    if TARGET.read_bytes() != SOURCE.read_bytes():
        raise SystemExit("Space CSV does not exactly match canonical selector-regimes CSV")

    print(
        f"Prepared VeloGraphX Space with {regime_count} regimes, "
        f"{sample_count} observations, {len(datasets)} datasets"
    )


if __name__ == "__main__":
    main()
