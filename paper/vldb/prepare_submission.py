#!/usr/bin/env python3
"""Prepare the PVLDB build directory from the canonical Markdown manuscript.

The script keeps `paper/manuscript.md` as the source of scientific prose while
allowing the official PVLDB template to compile in CI. It adds venue-facing
citations, reviewer-critical methodological disclosures, and evidence displays
from retained results, then Pandoc performs Markdown -> LaTeX conversion.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PAPER = ROOT / "paper"
BUILD = PAPER / "vldb" / "build"
MANUSCRIPT = PAPER / "manuscript.md"
RESULTS = PAPER / "data" / "accepted-results.json"


ABSTRACT_OVERRIDE = """Graph analytics systems increasingly operate on graphs that change continuously, yet exact maintenance is often committed in advance to either incremental repair or full recomputation. Neither execution mode dominates: localized repair can avoid most graph work when updates have limited structural impact, while recomputation can win once dependency discovery and repair become broad.

We present **VeloGraphX**, a C++20 system that treats localized repair and full recomputation as competing exact physical plans over one mutable graph substrate. The substrate combines segmented CSR, packed mutable deltas, sparse row patches, and forward/reverse adjacency. For dynamic BFS, VeloGraphX normalizes a batch to final edge states, identifies deleted shortest-path support before mutation, repairs invalidated state from valid boundary predecessors, propagates insertion-induced decreases, and retains conservative recomputation as a semantic safety mechanism. A separate pre-repair selector uses only pre-execution structural signals and prior measured arm costs to choose a plan before repair begins.

On a checksum-pinned three-graph validation covering nine regimes, 45 graph-regime repetitions, and 1,610 sequential batch observations, every output is exact. Equal-regime mean oracle regret is **3.94%**, versus **11.19%** for always-incremental, **17.84%** for a simple update-density threshold, and **20.21%** for a history-only cost model; sample-weighted regret is 2.31%, and selector decision cost is about 0.286 microseconds. The largest web-Google regime remains a visible tail at 17.48% mean regret. External results also reverse by workload: VeloGraphX is about 1.38x faster than NetworKit on the evaluated web-Google workload, while NetworKit is about 1.35x faster on ca-GrQc; a separate accepted RisGraph campaign retains a competitor win.

The conclusion is deliberately narrower than universal system superiority: exact dynamic analytics benefits from exposing repair and recomputation as observable, selectable physical plans whose preferred choice changes with graph and update regime."""


def section_between(text: str, start: str, end: str) -> str:
    a = text.index(start) + len(start)
    b = text.index(end, a)
    return text[a:b].strip()


def normalize_body(body: str) -> str:
    out: list[str] = []
    for line in body.splitlines():
        if line.startswith("### "):
            title = re.sub(r"^###\s+\d+(?:\.\d+)*\.?\s*", "## ", line)
            out.append(title)
        elif line.startswith("## "):
            title = re.sub(r"^##\s+\d+(?:\.\d+)*\.?\s*", "# ", line)
            out.append(title)
        else:
            out.append(line)
    return "\n".join(out).strip() + "\n"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def apply_deep_review_fixes(body: str) -> str:
    """Apply reviewer-driven claim-boundary fixes to the venue rendering.

    These edits do not change benchmark data. They correct the interpretation of
    the publication arm-isolation harness and make workload/statistical scope
    explicit in the PDF that reviewers actually read.
    """
    workload_anchor = "Machine-readable artifacts record environment and timing semantics."
    workload_note = workload_anchor + """

The primary selector workload is a **deterministic source-order sliding window**, not a claim of a naturally timestamped temporal trace. After the checksum-pinned imported prefix, each measured batch adds the next edges in the retained source order and removes the corresponding oldest edges from the active window. Source order, import fraction, root, and batch size are therefore part of the benchmark definition."""
    body = replace_once(body, workload_anchor, workload_note, "workload disclosure")

    stats_anchor = "Averages do not replace per-dataset or per-regime results when winner reversals occur."
    stats_note = stats_anchor + """

The 1,610 primary selector measurements are **sequential batch observations from 45 graph-regime repetitions**, not 1,610 independent experimental trials. We therefore use the mean across nine regimes as the primary cross-regime summary and report the sample-weighted mean separately; sample weighting gives more influence to small batches because they produce more observations. Tail percentiles are descriptive rather than inferential confidence bounds. The largest web-Google regime contributes 15 batch observations across five repetitions, so its p95 value is interpreted together with its mean, wrong-arm rate, and maximum."""
    body = replace_once(body, stats_anchor, stats_note, "nested-observation disclosure")

    correctness_anchor = "## System boundary beyond BFS"
    correctness = """## Correctness argument

The repair procedure is exact for the normalized final state of a batch. Consider first the intermediate graph containing all final deletions but not yet the final additions.

**Lemma 1 (unaffected vertices).** A reachable vertex is invalidated only when every old shortest-parent support is removed directly or transitively through an invalidated predecessor. Any vertex outside that closure retains at least one valid old shortest parent. Induction on old BFS level therefore preserves a path of the old length; deletions cannot create a shorter path, so its old distance remains exact.

**Lemma 2 (boundary repair).** Affected vertices are reset to unreachable, seeded with the best exact distance offered by unaffected predecessors, and relaxed through affected-to-affected edges in nondecreasing tentative distance. This is shortest-path relaxation on the affected subgraph with exact boundary labels, so every affected vertex obtains its exact distance in the deletion-only intermediate graph or remains unreachable.

**Lemma 3 (insertions).** Final inserted edges are then relaxed from exact intermediate distances, and every successful decrease is propagated until no relaxation remains. Insertions cannot increase distance, so standard unweighted relaxation yields the exact final post-batch distances, including newly reachable vertices.

**Theorem 1.** Batch normalization retains only the final desired state of each logical edge. Lemmas 1--3 establish exactness when localized repair completes; if the production affected-set bound is exceeded, VeloGraphX runs a fresh exact BFS. Thus the maintained distance vector equals exact recomputation after every normalized batch. Plan selection changes cost, not semantics.

""" + correctness_anchor
    body = replace_once(body, correctness_anchor, correctness, "correctness argument")

    selector_intro = "It evaluates checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google`, one fixed root per graph, three batch regimes per graph, five repetitions per regime, and one thread."
    selector_note = selector_intro + """

For this policy-comparison harness, the incremental arm is deliberately constructed with deletion fallback fraction **2.0**. Because the affected set cannot exceed the graph vertex count, this disables the ordinary 0.35 internal fallback and keeps always-incremental and always-full as clean experimental arms. The resulting zero internal-fallback count is therefore expected by construction and is **not** used as evidence that the selector prevented fallback. The production/default incremental BFS continues to use the 0.35 safety bound."""
    body = replace_once(body, selector_intro, selector_note, "fallback-isolation disclosure")

    old_metric = "The current selector records **3.939% mean oracle regret across regimes**, **2.309% sample-weighted mean regret**, a **1.739% sample-weighted wrong-arm rate**, **zero internal full fallbacks**, and about **0.286 µs sample-weighted decision cost**."
    new_metric = "The current selector records **3.939% mean oracle regret across regimes**, **2.309% sample-weighted mean regret**, a **1.739% sample-weighted wrong-arm rate**, and about **0.286 µs sample-weighted decision cost**."
    body = replace_once(body, old_metric, new_metric, "selector metric wording")

    old_fallback = """Zero internal fallbacks across the current-policy validation is also significant for the architecture. The adaptive path is not achieving low average regret by repeatedly entering incremental repair and then escaping to full BFS. Full executions recorded by the policy are explicit pre-repair choices. This separates selector quality from the incremental algorithm's 35% safety fallback and shows that the measured current-policy path avoids that particular form of double work on these experiments."""
    new_fallback = """Internal fallback is intentionally disabled in this policy-comparison harness so repair and recomputation remain separable oracle arms. Consequently, the zero-fallback count is not a selector-quality metric. The experiment establishes pre-repair plan quality under isolated exact arms; quantifying avoided repair-discovery-then-full work under the production 0.35 bound requires a separate deployment-style experiment."""
    body = replace_once(body, old_fallback, new_fallback, "fallback interpretation")

    old_double = """A selector that invokes incremental repair and only later discovers that the affected region is too large may pay twice: once for repair discovery or partial processing and again for full recomputation. This motivates pre-execution signals and selector-owned recomputation. The current publication-policy artifacts show zero internal fallbacks in the evaluated current-policy runs, so the policy is avoiding that specific double-work path there; the remaining error is mostly choosing the slower exact arm directly."""
    new_double = """A selector that invokes incremental repair and only later discovers that the affected region is too large may pay twice: once for repair discovery or partial processing and again for full recomputation. This motivates pre-execution signals and selector-owned recomputation. The current publication-policy harness intentionally disables internal fallback to isolate the repair and full arms, so it does **not** by itself measure avoided repair-then-full work. The current result is specifically a plan-selection result."""
    body = replace_once(body, old_double, new_double, "double-work claim boundary")

    policy_heading = "# Dynamic BFS versus NetworKit"
    policy_section = """# Policy baselines under one frozen harness

The retained cross-dataset artifact contains all policies under the same graph, root, batch, repetition, timing, and exactness contract. Equal weighting across nine regimes is important because small batches generate many more sequential observations than large batches. Always-incremental has 11.19% equal-regime mean regret, the simple update-density threshold 17.84%, the history-only cost model 20.21%, and the current adaptive policy 3.94%; their worst-regime mean regrets are 57.10%, 59.29%, 57.56%, and 17.48%, respectively.

The sample-weighted view deliberately reverses one comparison: always-incremental records 1.36% sample-weighted regret versus 2.31% for adaptive because observations are dominated by repair-friendly small-batch regimes. We retain that inversion rather than selecting only the favorable aggregate. The adaptive result is therefore best interpreted as **more robust across regimes**, not uniformly best under every weighting.

""" + policy_heading
    body = replace_once(body, policy_heading, policy_section, "policy-baseline section")

    nk_close = "The reversal is important: external-system conclusions are workload-specific. It also aligns with the paper's central premise that graph/update structure affects which execution machinery pays off. We do not generalize these two datasets to universal superiority."
    risgraph = nk_close + """

A separate accepted same-run web-Google campaign compares VeloGraphX with pinned official RisGraph under an exact incremental-BFS batch contract: 99% initial import, 4,096-edge batches, deterministic root 481807, 13 batches, and one thread. VeloGraphX localized repair averages 59.658 ms per answer-ready batch versus 31.333 ms for RisGraph, so **RisGraph is about 1.90x faster** on that workload. VeloGraphX repair is nevertheless about 1.98x faster than its own legacy 118.039 ms full-recompute path. This campaign is separate from the NetworKit campaign; its absolute times are never merged into a synthetic three-system ranking."""
    body = replace_once(body, nk_close, risgraph, "RisGraph visibility")

    limitation = "The current policy's three-graph validation uses a historical fixed graph/root program rather than a newly preregistered unseen holdout. Its average regret is low, but the largest `web-Google` regime has a material tail; the paper therefore does not claim uniform near-oracle behavior."
    limitation_new = "The current policy's three-graph validation uses a historical fixed graph/root program rather than a newly preregistered unseen holdout. Its workload is a deterministic source-order sliding window rather than a timestamped temporal trace, and its policy-comparison harness disables internal fallback to isolate the exact arms. Its average regret is low, but the largest `web-Google` regime has a material tail; the paper therefore does not claim uniform near-oracle behavior or that this experiment measures deployment fallback avoidance."
    body = replace_once(body, limitation, limitation_new, "limitations scope")
    return body


def make_typesetting_friendly(body: str) -> str:
    replacements = {
        "`always_incremental`": "always-incremental",
        "`always_full`": "always-full",
        "`simple_threshold`": "simple-threshold",
        "`history_cost_model`": "history-only cost model",
        "`ca-GrQc`": "ca-GrQc",
        "`soc-Epinions1`": "soc-Epinions1",
        "`web-Google`": "web-Google",
        "`p2p-Gnutella08`": "p2p-Gnutella08",
        "`ca-HepTh`": "ca-HepTh",
        "`facebook-combined`": "facebook-combined",
        "`com-Orkut`": "com-Orkut",
        "`G_t`": "$G_t$",
        "`U_t`": "$U_t$",
        "`G_{t-1}`": "$G_{t-1}$",
        "`F(G_t)`": "$F(G_t)$",
        "graph/reachability scale": "graph scale and reachability",
        "external-system conclusions are workload-specific": "external-system results vary by workload",
        "graph/update structure": "graph and update structure",
        "repair-versus-recompute selection": "repair/recompute selection",
        "dependency-driven and sparsity-aware incremental graph processing": "dependency- and sparsity-aware incremental graph processing",
    }
    for source, target in replacements.items():
        body = body.replace(source, target)
    body = re.sub(
        r"The primary current-policy campaign is GitHub Actions run `?\d+`? with retained artifact `?\d+`?\.",
        "The primary current-policy campaign is retained in the manuscript evidence registry.",
        body,
    )
    body = re.sub(
        r"A separate focused web-Google regression run \(`?\d+`?, artifact `?\d+`?\)",
        "A separate focused web-Google regression run retained in the evidence registry",
        body,
    )
    body = re.sub(r"`(\d{8,})`", r"\1", body)
    return body


def cite_first(text: str, phrase: str, key: str) -> str:
    marker = f"{phrase}\\cite{{{key}}}"
    if marker in text:
        return text
    if phrase not in text:
        raise RuntimeError(f"citation anchor not found: {phrase}")
    return text.replace(phrase, marker, 1)


def add_citations(body: str) -> str:
    citations = [
        ("GraphIn", "sengupta2016graphin"),
        ("Bok et al.", "bok2022cost"),
        ("GraphBolt", "mariappan2019graphbolt"),
        ("DZiG", "mariappan2021dzig"),
        ("RisGraph", "feng2021risgraph"),
        ("Layph", "yu2023layph"),
        ("NetworKit", "staudt2016networkit"),
        ("GAP Benchmark Suite", "beamer2015gap"),
        ("LAGraph/SuiteSparse:GraphBLAS", "szarnyas2021lagraph"),
    ]
    for phrase, key in citations:
        body = cite_first(body, phrase, key)

    storage_sentence = "The storage design separates a compact base representation from mutable state."
    storage_replacement = (
        "Prior dynamic graph stores such as GraphOne\\cite{kumar2019graphone} and "
        "Teseo\\cite{deleo2021teseo} already establish hybrid and mutable storage "
        "designs for evolving graphs. VeloGraphX does not claim mutable graph storage "
        "itself as new. " + storage_sentence
    )
    body = replace_once(body, storage_sentence, storage_replacement, "storage prior-work anchor")
    return body


def system_overview_figure() -> str:
    return r"""
\begin{figure*}[t]
\centering
\small
\begin{tabular}{c@{\;$\rightarrow$\;}c@{\;$\rightarrow$\;}c@{\;$\rightarrow$\;}c@{\;$\rightarrow$\;}c}
\fbox{Update batch} &
\fbox{Mutable graph substrate} &
\fbox{Pre-repair selector} &
\begin{tabular}{c}\fbox{Localized exact repair}\\[-1mm]\textit{or}\\[-1mm]\fbox{Exact full recompute}\end{tabular} &
\fbox{Exact result}
\end{tabular}
\\[1.5mm]
\begin{tabular}{c@{\hspace{12mm}}c@{\hspace{12mm}}c}
segmented CSR + deltas + row patches & cost / affected-work telemetry & bounded explicit consolidation
\end{tabular}
\caption{VeloGraphX exposes localized repair and full recomputation as exact physical plans over one mutable graph substrate. The selector uses only pre-execution state and prior telemetry; the selected arm changes cost, not semantics.}
\Description{System overview showing an update batch entering a mutable graph substrate, a pre-repair selector choosing localized repair or full recomputation, and both execution paths producing the same exact logical result. Telemetry feeds future decisions and storage consolidation is an explicit global maintenance operation.}
\label{fig:system-overview}
\end{figure*}
"""


def design_contract_table() -> str:
    return r"""
\begin{table*}[t]
\centering
\caption{VeloGraphX separates localized work from explicit global work at both storage and algorithm layers. Exactness is invariant; adaptation changes only the physical path.}
\label{tab:design-contract}
\begin{tabular}{p{0.14\textwidth}p{0.31\textwidth}p{0.27\textwidth}p{0.20\textwidth}}
\toprule
Layer & Localized path & Global path & Control / evidence \\
\midrule
Storage & Packed deltas and sparse row patches preserve untouched CSR rows & Canonical CSR + transpose consolidation & Bounded storage/latency maintenance policy \\
Dynamic BFS & Shortest-parent invalidation, boundary repair, insertion decrease propagation & Exact full BFS recomputation & Pre-repair selector; production default retains a 35\% affected-region fallback bound \\
Selection & Structural preflight plus recent arm-cost history & Direct choice of full execution before repair & Decision reason, predicted costs, decision time \\
Correctness & Maintained exact state & Independent exact reference & Verification outside timed publication regions \\
\bottomrule
\end{tabular}
\end{table*}
"""


def selector_workload_table() -> str:
    return r"""
\begin{table*}[t]
\centering
\caption{Primary selector workload. Each graph/regime has five repetitions. The stream is a deterministic source-order sliding window rather than a timestamped temporal trace.}
\label{tab:selector-workload}
\begin{tabular}{lrrrrl}
\toprule
Dataset & Vertices & Stream edges & Root & Initial import & Batch sizes \\
\midrule
ca-GrQc & 5,242 & 28,968 & 1,974 & 75\% & 96, 384, 1,536 \\
soc-Epinions1 & 75,879 & 508,837 & 71,391 & 99\% & 384, 1,536, 6,144 \\
web-Google & 875,713 & 5,105,039 & 391,806 & 99\% & 1,536, 6,144, 24,576 \\
\bottomrule
\end{tabular}
\end{table*}
"""


def policy_baseline_table() -> str:
    return r"""
\begin{table}[t]
\centering
\caption{Same-harness policy baselines. Equal-regime mean is the primary robustness summary; sample weighting favors small batches because they produce more observations.}
\label{tab:policy-baselines}
\begin{tabular}{lrrrr}
\toprule
Policy & Eq.-reg. & Weighted & Wrong arm & Worst reg. \\
\midrule
Always incremental & 11.19\% & \textbf{1.36\%} & 3.42\% & 57.10\% \\
Simple threshold & 17.84\% & 7.86\% & 8.39\% & 59.29\% \\
History-only model & 20.21\% & 11.52\% & 5.59\% & 57.56\% \\
Current adaptive & \textbf{3.94\%} & 2.31\% & \textbf{1.74\%} & \textbf{17.48\%} \\
\bottomrule
\end{tabular}
\end{table}
"""


def external_baseline_table(results: dict) -> str:
    nk = results["external_baselines"]["networkit_dynamic_bfs"]["datasets"]
    static = results["external_baselines"]["gap_lagraph_static"]
    web = 1.0 / nk["web-Google"]["velographx_over_networkit_latency_ratio"]
    grqc = nk["ca-GrQc"]["velographx_over_networkit_latency_ratio"]
    bfs_gap = static["bfs"]["velographx_vs_gap_speedup_range"]
    bfs_la = static["bfs"]["velographx_vs_lagraph_speedup_range"]
    sssp_gap = static["weighted_sssp"]["velographx_vs_gap_slowdown_range"]
    sssp_la = static["weighted_sssp"]["velographx_vs_lagraph_speedup_range"]
    return rf"""
\begin{{table*}}[t]
\centering
\caption{{Scoped external-baseline results from retained same-run or paired hosted campaigns. The RisGraph row is a separate campaign; absolute times are never combined across campaigns.}}
\label{{tab:external-baselines}}
\begin{{tabular}}{{llll}}
\toprule
Workload & Comparison & Evaluated result & Winner \\
\midrule
Dynamic BFS, web-Google & VeloGraphX vs NetworKit & {web:.2f}$\times$ lower latency & VeloGraphX \\
Dynamic BFS, ca-GrQc & VeloGraphX vs NetworKit & {grqc:.2f}$\times$ lower latency for NetworKit & NetworKit \\
Dynamic BFS, web-Google (separate) & VeloGraphX vs RisGraph & 1.90$\times$ lower latency for RisGraph & RisGraph \\
Static BFS, hosted 1--4 threads & VeloGraphX vs GAP & {bfs_gap[0]:.2f}--{bfs_gap[1]:.2f}$\times$ & VeloGraphX \\
Static BFS, hosted 1--4 threads & VeloGraphX vs LAGraph & {bfs_la[0]:.1f}--{bfs_la[1]:.1f}$\times$ & VeloGraphX \\
Weighted SSSP, hosted 1--4 threads & VeloGraphX vs GAP & {sssp_gap[0]:.1f}--{sssp_gap[1]:.1f}$\times$ slower & GAP \\
Weighted SSSP, hosted 1--4 threads & VeloGraphX vs LAGraph & {sssp_la[0]:.1f}--{sssp_la[1]:.1f}$\times$ & VeloGraphX \\
\bottomrule
\end{{tabular}}
\end{{table*}}
"""


def storage_table(results: dict) -> str:
    r = results["orkut_canonicalization_ab"]
    a = r["conservative"]
    b = r["large_graph"]
    d = r["derived"]
    return rf"""
\begin{{table}}[t]
\centering
\caption{{com-Orkut canonicalization-policy A/B (234.4M directed arcs, 60 epochs).}}
\label{{tab:orkut-storage}}
\begin{{tabular}}{{lrr}}
\toprule
Metric & 1.25$\times$ & 1.50$\times$ \\
\midrule
Consolidations & {a['consolidations']} & {b['consolidations']} \\
Consolidation time (s) & {a['total_consolidation_seconds']:.1f} & {b['total_consolidation_seconds']:.1f} \\
Maintenance throughput (ops/s) & {a['maintenance_amortized_ops_per_second']:,} & {b['maintenance_amortized_ops_per_second']:,} \\
Peak RSS (GiB) & {a['peak_rss_kib']/(1024*1024):.2f} & {b['peak_rss_kib']/(1024*1024):.2f} \\
\bottomrule
\end{{tabular}}
\\[-1mm]
\footnotesize {d['throughput_speedup']:.2f}$\times$ throughput with {100*d['peak_rss_increase_fraction']:.1f}\% higher peak RSS.
\end{{table}}
"""


def insert_before_heading(body: str, heading: str, display: str) -> str:
    pattern = re.compile(rf"(?m)^{re.escape(heading)}$")
    matches = list(pattern.finditer(body))
    if len(matches) != 1:
        raise RuntimeError(f"display anchor must match exactly once: {heading!r}; found {len(matches)}")
    m = matches[0]
    return body[:m.start()] + display.strip() + "\n\n" + body[m.start():]


def inject_displays(body: str, results: dict) -> str:
    selector_fig = r"""
\begin{figure*}[t]
  \centering
  \includegraphics[width=0.94\textwidth]{figures/selector-regret.pdf}
  \caption{Current publication selector across three graph families and three update regimes per graph. Top: mean and p95 oracle-relative regret. Bottom: wrong-arm choices and explicit pre-repair full choices. Internal fallback is intentionally disabled in this policy-comparison harness and is not selector evidence.}
  \Description{Two-panel chart over nine graph and update regimes. The top panel shows mean and p95 oracle-relative regret; the lower panel shows wrong-arm and explicit full-choice percentages. Most regimes have low regret, while the largest web-Google regime has the largest regret tail and a one-third wrong-arm rate.}
  \label{fig:selector-regret}
\end{figure*}
"""
    triangle_fig = r"""
\begin{figure}[t]
  \centering
  \includegraphics[width=\linewidth]{figures/triangle-exact-reference.pdf}
  \caption{Exact dynamic triangle answer-ready latency against the pinned GoldenCounter reference on facebook-combined. All 15 paired executions are exact.}
  \Description{Bar chart comparing VeloGraphX and GoldenCounter exact answer-ready latency at one, five, and ten percent insertion batches.}
  \label{fig:triangle-reference}
\end{figure}
"""

    body = insert_before_heading(body, "# System design", system_overview_figure())
    body = insert_before_heading(body, "## Exact deletion repair", design_contract_table())
    body = insert_before_heading(body, "## Reproducibility discipline", selector_workload_table())
    body = insert_before_heading(body, "# Policy baselines under one frozen harness", selector_fig)
    body = insert_before_heading(body, "# Dynamic BFS versus NetworKit", policy_baseline_table())
    body = insert_before_heading(body, "# Static BFS and SSSP versus GAP and LAGraph", external_baseline_table(results))
    body = insert_before_heading(body, "# Large-graph storage maintenance", triangle_fig)
    body = insert_before_heading(body, "# Supporting breadth and maturity", storage_table(results))

    orphan = re.search(r"(?m)^#{1,6}\s*$", body)
    if orphan:
        raise RuntimeError(f"orphan Markdown heading after display injection near offset {orphan.start()}")
    return body


def main() -> None:
    text = MANUSCRIPT.read_text(encoding="utf-8")
    results = json.loads(RESULTS.read_text(encoding="utf-8"))
    BUILD.mkdir(parents=True, exist_ok=True)

    body_start = text.index("## 1. Introduction")
    body = normalize_body(text[body_start:])
    body = apply_deep_review_fixes(body)
    body = make_typesetting_friendly(body)
    body = add_citations(body)
    body = inject_displays(body, results)

    (BUILD / "abstract.md").write_text(ABSTRACT_OVERRIDE.strip() + "\n", encoding="utf-8")
    (BUILD / "body.md").write_text(body, encoding="utf-8")

    print(f"Prepared cited manuscript, reviewer disclosures, and evidence displays under {BUILD}")


if __name__ == "__main__":
    main()
