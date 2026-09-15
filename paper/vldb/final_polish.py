#!/usr/bin/env python3
"""Apply final reviewer-facing PVLDB corrections after canonical preparation.

This pass consumes only committed paper data and the already prepared Markdown
body. It does not change benchmark measurements or selector behavior.
"""

from __future__ import annotations

import csv
import json
import re
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


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def pct(x: float) -> str:
    return f"{100.0 * x:.2f}\\%"


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
    fig.suptitle("Repair/recompute crossover and current selector quality", fontsize=11)
    fig.tight_layout()
    FIGURES.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIGURES / "selector-crossover.pdf", bbox_inches="tight")
    fig.savefig(FIGURES / "selector-crossover.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def selector_spec_table() -> str:
    return r"""
\begin{table*}[t]
\centering
\caption{Frozen publication-selector constants. Reachability is measured once after the initial exact BFS; the policy consumes no oracle information before choosing an arm.}
\label{tab:selector-spec}
\begin{tabular}{p{0.23\textwidth}p{0.70\textwidth}}
\toprule
Component & Frozen rule \\
\midrule
Common history & EMA weight $\alpha=0.25$; an arm observation is fresh for at most 4 batches; update fraction $\geq5\%$ selects full recomputation. \\
Smaller graphs ($|V|<200{,}000$) & Initial reachable fraction $\leq2\%$ selects full; if reachability $<20\%$, update fraction $\geq0.25\%$ selects full. With fresh observations, select full only when predicted incremental cost exceeds $1.25\times$ predicted full cost; the incremental prediction is multiplied by $1+2a_{t-1}$, where $a_{t-1}$ is the previous affected-vertex fraction. \\
Large graphs ($|V|\geq200{,}000$) & The first batch selects full when any shallow shortest-parent deletion is observed. Before an incremental observation exists, probe incremental rather than paying a redundant full calibration. Later, scale the incremental EMA by $\mathrm{clamp}(\sqrt{u_t/u_{\mathrm{last}}},0.5,2.5)(1+3a_{t-1})$. \\
Large-graph uncertainty & Incremental relative-error uncertainty defaults to 35\%, full uncertainty to 20\%, and observed uncertainties are clamped to 5--35\%. Select full only when the incremental lower-cost bound exceeds the full upper-cost bound; overlapping bounds default to incremental. \\
\bottomrule
\end{tabular}
\end{table*}
"""


def policy_table(derived: dict) -> str:
    p = derived["selector_plan_comparison"]["policy_aggregates"]
    names = [
        ("Always incremental", "always_incremental"),
        ("Always full", "always_full"),
        ("Simple threshold", "simple_threshold"),
        ("History-only model", "history_cost_model"),
        ("Current adaptive", "adaptive"),
    ]
    lines = []
    for label, key in names:
        r = p[key]
        lines.append(
            f"{label} & {pct(r['equal_regime_mean_regret'])} & "
            f"{pct(r['sample_weighted_mean_regret'])} & "
            f"{pct(r['sample_weighted_wrong_arm_rate'])} & "
            f"{pct(r['worst_regime_mean_regret'])} \\\\"
        )
    return r"""
\begin{table*}[t]
\centering
\caption{Same-harness policy baselines. Equal-regime mean is the primary robustness summary. Weighted regret and wrong-arm rate are sample-weighted and therefore emphasize small batches, which produce more observations.}
\label{tab:policy-baselines}
\begin{tabular}{lrrrr}
\toprule
Policy & Eq.-reg. mean regret & Weighted regret & Weighted wrong arm & Worst-regime mean \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table*}
"""


def polish_body(body: str, derived: dict) -> str:
    old_rq2 = "**RQ2 — Selector behavior.** When the policy makes a wrong choice, is the error frequent, expensive, or concentrated in particular regimes? Does it avoid repair-then-full double work?"
    new_rq2 = "**RQ2 — Selector behavior.** When the policy makes a wrong choice, is the error frequent, expensive, or concentrated in particular regimes, and how does the frozen adaptive policy compare with simpler policies under the same harness?"
    body = replace_once(body, old_rq2, new_rq2, "RQ2 scope")

    body = replace_once(
        body,
        "soc-Epinions1 & 75,879 & 508,837 & 71,391 & 99\\% & 384, 1,536, 6,144",
        "soc-Epinions1 & 75,879 & 508,837 & 71,391 & 90\\% & 384, 1,536, 6,144",
        "Epinions import fraction",
    )

    body = replace_once(
        body,
        "## Cost history and freshness",
        selector_spec_table().strip() + "\n\n## Cost history and freshness",
        "selector specification table",
    )

    table_pattern = re.compile(
        r"\\begin\{table\}\[t\].*?\\caption\{Same-harness policy baselines\..*?\\end\{table\}", re.S
    )
    body, n = table_pattern.subn(lambda _m: policy_table(derived).strip(), body, count=1)
    if n != 1:
        raise RuntimeError(f"policy table replacement: expected 1 match, found {n}")

    baseline_anchor = (
        "The sample-weighted view deliberately reverses one comparison: always-incremental records 1.36% sample-weighted regret versus 2.31% for adaptive because observations are dominated by repair-friendly small-batch regimes. We retain that inversion rather than selecting only the favorable aggregate. The adaptive result is therefore best interpreted as **more robust across regimes**, not uniformly best under every weighting."
    )
    body = replace_once(
        body,
        baseline_anchor,
        baseline_anchor + " Always-full is also retained in Table 3 for completeness; its 620.29% equal-regime mean regret is driven by the small-update regimes where a graph-wide traversal is far more expensive than repair.",
        "always-full interpretation",
    )

    fig_pattern = re.compile(
        r"\\begin\{figure\*\}\[t\]\s*\\centering\s*\\includegraphics\[width=0\.94\\textwidth\]\{figures/selector-regret\.pdf\}.*?\\end\{figure\*\}", re.S
    )
    new_fig = r"""
\begin{figure*}[t]
  \centering
  \includegraphics[width=0.94\textwidth]{figures/selector-crossover.pdf}
  \caption{Repair/recompute crossover and selector quality under the frozen nine-regime harness. Top: median paired always-incremental/full answer-ready latency ratio; values below 1 favor repair and values above 1 favor full recomputation. Bottom: current adaptive mean and p95 oracle-relative regret. The figure exposes both the physical-plan crossover and the visible large-web-Google selector tail.}
  \Description{Two-panel chart across nine graph/update regimes. The top panel plots the median paired incremental-to-full latency ratio with a crossover line at one; ca-GrQc at batch 1,536 and web-Google at batch 24,576 favor full recomputation while the other evaluated regimes favor repair. The lower panel shows adaptive mean and p95 regret, including the visible large-web-Google tail.}
  \label{fig:selector-crossover}
\end{figure*}
"""
    body, n = fig_pattern.subn(lambda _m: new_fig.strip(), body, count=1)
    if n != 1:
        raise RuntimeError(f"selector figure replacement: expected 1 match, found {n}")

    # Match after citation and Markdown-emphasis injection, not a brittle exact sentence.
    nk_pattern = re.compile(
        r"On ca-GrQc, the ratio is about 1\.35, so \*\*NetworKit(?:\\cite\{staudt2016networkit\})? is approximately 1\.35× faster\*\*\."
    )
    nk_note = (
        " Across the three audited roots, mean paired VeloGraphX/NetworKit ratios range "
        "from 0.70–0.76 on web-Google and 1.30–1.41 on ca-GrQc, so the reversal is not "
        "driven by one selected root."
    )
    body, n = nk_pattern.subn(lambda m: m.group(0) + nk_note, body, count=1)
    if n != 1:
        raise RuntimeError(f"NetworKit dispersion: expected 1 sentence match, found {n}")

    storage_note = "\\\\[-1mm]\n\\footnotesize 2.25$\\times$ throughput with 6.6\\% higher peak RSS.\n"
    body = replace_once(body, storage_note, "", "storage table note cleanup")

    replacements = {
        "The accepted NetworKit": "The audited NetworKit",
        "A separate accepted same-run web-Google campaign": "A separate audited same-run web-Google campaign",
        "The accepted canonicalization A/B": "The audited canonicalization A/B",
        "Historical one-sided warm-up behavior is retained in development provenance, while the publication validation records that the obsolete redundant one-sided-full choice is absent in the current audited policy.": "The publication validation records an explicit decision reason for every batch and retains policy errors without post-hoc threshold retuning.",
        "The obsolete redundant one-sided-full decision is never taken.": "Every adaptive decision retains an explicit reason code.",
        " Python and other 0.x APIs may evolve.": "",
    }
    for old, new in replacements.items():
        body = replace_once(body, old, new, f"final-language cleanup: {old[:30]}")

    dynamic_anchor = "These systems reinforce that dynamic graph performance depends on how change propagates; VeloGraphX studies the complementary plan-selection question under exact repair/recompute alternatives."
    body = replace_once(
        body,
        dynamic_anchor,
        dynamic_anchor + r"""

Differential Dataflow\cite{mcsherry2013differential} provides a broader incremental-dataflow model for maintaining iterative computations under changing inputs. It reinforces that incremental computation can be a first-class execution model, but it does not address VeloGraphX's narrower decision problem of selecting between an exact localized graph repair plan and an exact full-recompute plan before repair begins. Accordingly, our novelty claim is not the existence of dual paths, historical cost signals, or incremental computation in isolation; it is their integration with exact repair semantics, selector/fallback separation, shared mutable graph state, and per-batch measured-oracle telemetry.""",
        "incremental related work",
    )

    storage_anchor = "Dynamic storage itself is not claimed as novel. GraphOne uses a hybrid representation supporting graph updates and analytical views, while Teseo develops a sophisticated mutable graph representation with transactional support."
    body = replace_once(
        body,
        storage_anchor,
        storage_anchor + r""" LLAMA\cite{macko2015llama} uses multiversioned arrays for graph analytics over evolving state; LiveGraph\cite{zhu2020livegraph} targets transactional updates while retaining sequential adjacency scans; and Sortledton\cite{fuchs2022sortledton} develops a universal transactional graph structure for updates and analytical access. More recent systems continue to explore this design space: Spruce\cite{shi2024spruce} emphasizes update throughput and space efficiency, LSMGraph\cite{yu2024lsmgraph} combines LSM-style update handling with multi-level CSR for dynamic graph storage, a 2025 SIGMOD study\cite{su2025dynamicstorage} systematically revisits in-memory dynamic-storage trade-offs, and RadixGraph\cite{xie2026radixgraph} uses a space-optimized radix index with snapshot/log-style edge storage.""",
        "storage related work",
    )
    return body


def main() -> None:
    derived = json.loads(DERIVED.read_text(encoding="utf-8"))
    generate_crossover_figure(derived)

    body = polish_body(BODY.read_text(encoding="utf-8"), derived)
    BODY.write_text(body, encoding="utf-8")

    abstract = ABSTRACT.read_text(encoding="utf-8")
    abstract = replace_once(
        abstract,
        "a separate accepted RisGraph campaign retains a competitor win",
        "a separate audited RisGraph campaign retains a competitor win",
        "abstract campaign wording",
    )
    ABSTRACT.write_text(abstract, encoding="utf-8")

    forbidden = [
        "Does it avoid repair-then-full double work?",
        "selector-regret.pdf",
        "accepted same-run",
        "accepted canonicalization",
        "Python and other 0.x APIs may evolve",
        "soc-Epinions1 & 75,879 & 508,837 & 71,391 & 99\\%",
    ]
    for phrase in forbidden:
        if phrase in body:
            raise RuntimeError(f"stale reviewer-facing text remains: {phrase}")

    print("Applied final reviewer corrections and generated selector crossover figure")


if __name__ == "__main__":
    main()
