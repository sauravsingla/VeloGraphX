#!/usr/bin/env python3
"""Final non-scientific PVLDB build polish.

The canonical manuscript now contains the reviewer-facing scientific claims, so
this stage deliberately avoids brittle sentence-by-sentence rewrites.  It only
generates the evidence-derived crossover figure, performs harmless wording
cleanup, and validates that the synchronized manuscript contains the required
claim boundaries before LaTeX compilation.
"""

from __future__ import annotations

import csv
import json
from pathlib import Path

import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[2]
PAPER = ROOT / "paper"
BUILD = PAPER / "vldb" / "build"
BODY = BUILD / "body.md"
ABSTRACT = BUILD / "abstract.md"
DERIVED = PAPER / "data" / "final-review-derived.json"
SELECTOR_ROWS = PAPER / "data" / "current-selector-regimes.csv"
FIGURES = PAPER / "figures" / "generated"


def generate_crossover_figure(derived: dict) -> None:
    rows = derived["selector_plan_comparison"]["rows"]
    with SELECTOR_ROWS.open(newline="") as f:
        selector = list(csv.DictReader(f))
    sel = {(r["dataset"], int(r["batch_size"])): r for r in selector}

    order = []
    for dataset in ("ca-GrQc", "soc-Epinions1", "web-Google"):
        order.extend(sorted((r for r in rows if r["dataset"] == dataset), key=lambda r: r["batch_size"]))

    abbrev = {"ca-GrQc": "GrQc", "soc-Epinions1": "Epinions", "web-Google": "Google"}
    labels, ratios, means, p95s = [], [], [], []
    for r in order:
        key = (r["dataset"], int(r["batch_size"]))
        if key not in sel:
            raise RuntimeError(f"missing selector-regime row for {key}")
        labels.append(f"{abbrev[r['dataset']]}\n{int(r['batch_size']):,}")
        ratios.append(float(r["median_incremental_over_full_ratio"]))
        means.append(100.0 * float(sel[key]["mean_regret"]))
        p95s.append(100.0 * float(sel[key]["p95_regret"]))

    x = list(range(len(order)))
    fig, (ax_ratio, ax_regret) = plt.subplots(
        2, 1, figsize=(8.2, 5.8), sharex=True,
        gridspec_kw={"height_ratios": [1.0, 1.35]},
    )
    ax_ratio.bar(x, ratios, label="Median incremental/full latency ratio")
    ax_ratio.axhline(1.0, linestyle="--", linewidth=1.0, label="Crossover (1.0)")
    ax_ratio.set_ylabel("Inc. / full latency")
    ax_ratio.set_ylim(0, max(1.75, max(ratios) * 1.08))
    ax_ratio.grid(axis="y", alpha=0.25)
    ax_ratio.legend(ncol=2, fontsize=8, loc="upper left")
    for i, value in enumerate(ratios):
        if value > 1.0:
            ax_ratio.annotate("full wins", (i, value), xytext=(0, 5), textcoords="offset points", ha="center", fontsize=7)

    width = 0.36
    ax_regret.bar([i - width / 2 for i in x], means, width, label="Adaptive mean regret")
    ax_regret.bar([i + width / 2 for i in x], p95s, width, label="Adaptive p95 regret")
    ax_regret.set_ylabel("Oracle-relative regret (%)")
    ax_regret.set_xlabel("Graph and batch size")
    ax_regret.set_xticks(x, labels, rotation=18, ha="right")
    ax_regret.grid(axis="y", alpha=0.25)
    ax_regret.legend(ncol=2, fontsize=8)
    worst = max(range(len(p95s)), key=lambda i: p95s[i])
    ax_regret.annotate(
        "visible tail", (worst + width / 2, p95s[worst]), xytext=(-28, 10),
        textcoords="offset points", arrowprops={"arrowstyle": "->"}, fontsize=7,
    )
    fig.tight_layout()
    FIGURES.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIGURES / "selector-crossover.pdf", bbox_inches="tight")
    fig.savefig(FIGURES / "selector-crossover.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def validate_synchronized_text(body: str, abstract: str) -> None:
    required = {
        "production fallback evidence": "17.323 ms",
        "held-out negative result": "CollegeMsg",
        "held-out positive result": "Amazon0312",
        "clean ablation": "previous-affected-work",
        "matched GraphBolt comparison": "GraphBolt",
        "PageRank claim boundary": "residual/tolerance",
        "selector tail": "54.424% p95 regret",
    }
    combined = abstract + "\n" + body
    missing = [label for label, phrase in required.items() if phrase not in combined]
    if missing:
        raise RuntimeError("synchronized manuscript is missing required evidence: " + ", ".join(missing))
    forbidden = [
        "selector-regret.pdf",
        "soc-Epinions1 & 75,879 & 508,837 & 71,391 & 99\\%",
        "current publication-policy artifacts show zero internal fallbacks",
    ]
    stale = [phrase for phrase in forbidden if phrase in combined]
    if stale:
        raise RuntimeError("stale pre-closure manuscript/build text remains: " + "; ".join(stale))


def main() -> None:
    derived = json.loads(DERIVED.read_text(encoding="utf-8"))
    generate_crossover_figure(derived)

    body = BODY.read_text(encoding="utf-8")
    abstract = ABSTRACT.read_text(encoding="utf-8")
    body = body.replace("The accepted NetworKit campaign", "The audited NetworKit campaign")
    BODY.write_text(body, encoding="utf-8")

    validate_synchronized_text(body, abstract)
    print("PVLDB final polish passed: synchronized claims validated and crossover figure generated")


if __name__ == "__main__":
    main()
