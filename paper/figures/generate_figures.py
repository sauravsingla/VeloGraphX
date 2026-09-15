#!/usr/bin/env python3
"""Generate manuscript figures strictly from committed paper evidence.

The script intentionally reads only files under paper/data/. It does not scrape README
text or GitHub Actions logs, which keeps figures tied to the audited manuscript ledger.
"""

from __future__ import annotations

import csv
import json
from pathlib import Path

import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
OUT = Path(__file__).resolve().parent / "generated"
OUT.mkdir(parents=True, exist_ok=True)


def load_selector_rows():
    with (DATA / "current-selector-regimes.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))
    numeric = {
        "batch_size": int,
        "repetitions": int,
        "samples": int,
        "median_batch_us": float,
        "mean_regret": float,
        "p95_regret": float,
        "max_regret": float,
        "wrong_arm_rate": float,
        "full_choice_fraction": float,
        "internal_fallback_fraction": float,
        "mean_decision_us": float,
    }
    for row in rows:
        for field, cast in numeric.items():
            row[field] = cast(row[field])
    return rows


def selector_regret_figure(rows):
    fig, ax = plt.subplots(figsize=(7.2, 4.3))
    for dataset in sorted({r["dataset"] for r in rows}):
        rs = sorted((r for r in rows if r["dataset"] == dataset), key=lambda r: r["batch_size"])
        xs = list(range(len(rs)))
        mean = [100.0 * r["mean_regret"] for r in rs]
        p95 = [100.0 * r["p95_regret"] for r in rs]
        labels = [f'{dataset}\n{r["batch_size"]}' for r in rs]
        ax.plot(xs, mean, marker="o", label=f"{dataset} mean")
        ax.plot(xs, p95, marker="x", linestyle="--", label=f"{dataset} p95")
        # Each dataset has its own batch scale; textual labels prevent implying common x values.
        for x, y, label in zip(xs, mean, labels):
            ax.annotate(label, (x, y), xytext=(0, 6), textcoords="offset points", ha="center", fontsize=7)
    ax.set_ylabel("Oracle-relative regret (%)")
    ax.set_xlabel("Three increasing batch regimes per graph (labels show graph and batch size)")
    ax.set_title("Current publication selector: mean and p95 regret")
    ax.grid(axis="y", alpha=0.25)
    ax.legend(fontsize=8, ncol=2)
    fig.tight_layout()
    fig.savefig(OUT / "selector-regret.pdf", bbox_inches="tight")
    fig.savefig(OUT / "selector-regret.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def triangle_reference_figure(results):
    rows = results["exact_triangles_published_reference"]["rows"]
    labels = [f'{int(r["update_fraction"] * 100)}%' for r in rows]
    vx = [r["velographx_answer_ready_ms"] for r in rows]
    gc = [r["goldencounter_answer_ready_ms"] for r in rows]
    x = list(range(len(rows)))
    width = 0.38

    fig, ax = plt.subplots(figsize=(6.4, 4.0))
    ax.bar([i - width / 2 for i in x], vx, width, label="VeloGraphX")
    ax.bar([i + width / 2 for i in x], gc, width, label="GoldenCounter exact reference")
    ax.set_xticks(x, labels)
    ax.set_xlabel("Insertion batch fraction")
    ax.set_ylabel("Median exact answer-ready latency (ms)")
    ax.set_title("Exact dynamic triangle answer-ready latency")
    ax.legend()
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(OUT / "triangle-exact-reference.pdf", bbox_inches="tight")
    fig.savefig(OUT / "triangle-exact-reference.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def canonicalization_figure(results):
    r = results["orkut_canonicalization_ab"]
    conservative = r["conservative"]
    large = r["large_graph"]
    metrics = [
        ("Consolidations", conservative["consolidations"], large["consolidations"]),
        ("Consolidation time (s)", conservative["total_consolidation_seconds"], large["total_consolidation_seconds"]),
        ("Throughput (kops/s)", conservative["maintenance_amortized_ops_per_second"] / 1000.0, large["maintenance_amortized_ops_per_second"] / 1000.0),
        ("Peak RSS (GiB)", conservative["peak_rss_kib"] / (1024.0 * 1024.0), large["peak_rss_kib"] / (1024.0 * 1024.0)),
    ]
    # Separate files avoid putting incomparable units on one shared y-axis.
    for name, a, b in metrics:
        slug = name.lower().replace(" ", "-").replace("(", "").replace(")", "").replace("/", "-")
        fig, ax = plt.subplots(figsize=(4.6, 3.5))
        ax.bar([0, 1], [a, b])
        ax.set_xticks([0, 1], ["1.25× envelope", "1.50× envelope"])
        ax.set_ylabel(name)
        ax.set_title(f"com-Orkut: {name}")
        ax.grid(axis="y", alpha=0.25)
        fig.tight_layout()
        fig.savefig(OUT / f"orkut-{slug}.pdf", bbox_inches="tight")
        fig.savefig(OUT / f"orkut-{slug}.png", dpi=220, bbox_inches="tight")
        plt.close(fig)


def main():
    selector_rows = load_selector_rows()
    results = json.loads((DATA / "accepted-results.json").read_text())
    selector_regret_figure(selector_rows)
    triangle_reference_figure(results)
    canonicalization_figure(results)
    print(f"Generated manuscript figures under {OUT}")


if __name__ == "__main__":
    main()
