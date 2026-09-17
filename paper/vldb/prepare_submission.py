#!/usr/bin/env python3
"""Prepare the PVLDB build tree from the canonical Markdown manuscript.

`paper/manuscript.md` is the scientific source of truth.  This script performs
only venue-facing transformations: heading normalization, citations, and
insertion of evidence-derived figures/tables.  Reviewer conclusions and
benchmark claims must live in the manuscript/evidence registries, not in brittle
post-processing replacements.
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
DERIVED = PAPER / "data" / "final-review-derived.json"


def section_between(text: str, start: str, end: str) -> str:
    a = text.index(start) + len(start)
    b = text.index(end, a)
    return text[a:b].strip()


def normalize_body(body: str) -> str:
    out: list[str] = []
    for line in body.splitlines():
        if line.startswith("### "):
            out.append(re.sub(r"^###\s+\d+(?:\.\d+)*\.?\s*", "## ", line))
        elif line.startswith("## "):
            out.append(re.sub(r"^##\s+\d+(?:\.\d+)*\.?\s*", "# ", line))
        else:
            out.append(line)
    return "\n".join(out).strip() + "\n"


def make_typesetting_friendly(body: str) -> str:
    replacements = {
        "`always_incremental`": "always-incremental",
        "`always_full`": "always-full",
        "`simple_threshold`": "simple-threshold",
        "`history_cost_model`": "history-only cost model",
        "`ca-GrQc`": "ca-GrQc",
        "`soc-Epinions1`": "soc-Epinions1",
        "`web-Google`": "web-Google",
        "`Amazon0312`": "Amazon0312",
        "`CollegeMsg`": "CollegeMsg",
        "`facebook-combined`": "facebook-combined",
        "`com-Orkut`": "com-Orkut",
        "`GoldenCounter`": "GoldenCounter",
        "`publication-preflight-v1`": "publication-preflight-v1",
        "`G_t`": "$G_t$",
        "`U_t`": "$U_t$",
        "`G_{t-1}`": "$G_{t-1}$",
        "`F(G_t)`": "$F(G_t)$",
    }
    for source, target in replacements.items():
        body = body.replace(source, target)
    body = re.sub(r"`(\d{8,})`", r"\1", body)
    return body


def cite_first_if_present(text: str, phrase: str, key: str) -> str:
    marker = f"{phrase}\\cite{{{key}}}"
    if marker in text or phrase not in text:
        return text
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.lstrip().startswith("#"):
            continue
        if phrase in line:
            lines[i] = line.replace(phrase, marker, 1)
            return "".join(lines)
    return text


def add_citations(body: str) -> str:
    citations = [
        ("GraphIn", "sengupta2016graphin"),
        ("Bok et al.", "bok2022cost"),
        ("GraphBolt", "mariappan2019graphbolt"),
        ("DZiG", "mariappan2021dzig"),
        ("RisGraph", "feng2021risgraph"),
        ("Layph", "yu2023layph"),
        ("GraphOne", "kumar2019graphone"),
        ("Teseo", "deleo2021teseo"),
        ("NetworKit", "staudt2016networkit"),
        ("GAP Benchmark Suite", "beamer2015gap"),
        ("LAGraph/SuiteSparse:GraphBLAS", "szarnyas2021lagraph"),
    ]
    for phrase, key in citations:
        body = cite_first_if_present(body, phrase, key)
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
\Description{System overview showing an update batch entering a mutable graph substrate, a pre-repair selector choosing localized repair or full recomputation, and both execution paths producing the same exact logical result.}
\label{fig:system-overview}
\end{figure*}
"""


def design_contract_table() -> str:
    return r"""
\begin{table*}[t]
\centering
\caption{VeloGraphX separates localized work from explicit global work. Exactness is invariant; adaptation changes only the physical path.}
\label{tab:design-contract}
\begin{tabular}{p{0.14\textwidth}p{0.31\textwidth}p{0.27\textwidth}p{0.20\textwidth}}
\toprule
Layer & Localized path & Global path & Control / evidence \\
\midrule
Storage & Packed deltas and sparse row patches preserve untouched CSR rows & Canonical CSR + transpose consolidation & Bounded maintenance policy \\
Dynamic BFS & Shortest-parent invalidation, boundary repair, insertion decrease propagation & Exact full BFS recomputation & Pre-repair selector; production 35\% affected-region fallback bound \\
Selection & Structural preflight plus recent arm-cost history & Direct full choice before repair & Decision reason, predicted costs, decision time \\
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
soc-Epinions1 & 75,879 & 508,837 & 71,391 & 90\% & 384, 1,536, 6,144 \\
web-Google & 875,713 & 5,105,039 & 391,806 & 99\% & 1,536, 6,144, 24,576 \\
\bottomrule
\end{tabular}
\end{table*}
"""


def pct(x: float) -> str:
    return f"{100.0 * x:.2f}\\%"


def policy_baseline_table(derived: dict) -> str:
    p = derived["selector_plan_comparison"]["policy_aggregates"]
    names = [
        ("Always incremental", "always_incremental"),
        ("Always full", "always_full"),
        ("Simple threshold", "simple_threshold"),
        ("History-only model", "history_cost_model"),
        ("Current adaptive", "adaptive"),
    ]
    rows = []
    for label, key in names:
        r = p[key]
        rows.append(
            f"{label} & {pct(r['equal_regime_mean_regret'])} & "
            f"{pct(r['sample_weighted_mean_regret'])} & "
            f"{pct(r['sample_weighted_wrong_arm_rate'])} & "
            f"{pct(r['worst_regime_mean_regret'])} \\\\"
        )
    return r"""
\begin{table*}[t]
\centering
\caption{Same-harness policy baselines. Equal-regime mean is the primary robustness summary; weighted metrics emphasize small batches because they produce more sequential observations.}
\label{tab:policy-baselines}
\begin{tabular}{lrrrr}
\toprule
Policy & Eq.-reg. mean regret & Weighted regret & Weighted wrong arm & Worst-regime mean \\
\midrule
""" + "\n".join(rows) + r"""
\bottomrule
\end{tabular}
\end{table*}
"""


def external_baseline_table(results: dict) -> str:
    nk = results["external_baselines"]["networkit_dynamic_bfs"]["datasets"]
    static = results["external_baselines"]["gap_lagraph_static"]
    graphbolt = results["external_baselines"]["graphbolt_dynamic_bfs"]["graphbolt_over_velographx_answer_ready_ratio"]
    web = 1.0 / nk["web-Google"]["velographx_over_networkit_latency_ratio"]
    grqc = nk["ca-GrQc"]["velographx_over_networkit_latency_ratio"]
    bfs_gap = static["bfs"]["velographx_vs_gap_speedup_range"]
    bfs_la = static["bfs"]["velographx_vs_lagraph_speedup_range"]
    sssp_gap = static["weighted_sssp"]["velographx_vs_gap_slowdown_range"]
    sssp_la = static["weighted_sssp"]["velographx_vs_lagraph_speedup_range"]
    return rf"""
\begin{{table*}}[t]
\centering
\caption{{Scoped external-baseline results. Winner reversals are retained; unrelated hosted campaigns are not combined into an absolute ranking.}}
\label{{tab:external-baselines}}
\begin{{tabular}}{{llll}}
\toprule
Workload & Comparison & Evaluated result & Winner \\
\midrule
Matched dynamic BFS, 0.01\% updates & GraphBolt/VeloGraphX latency & {graphbolt['0.0001']:.3f}$\times$ & VeloGraphX \\
Matched dynamic BFS, 0.5\% updates & GraphBolt/VeloGraphX latency & {graphbolt['0.005']:.3f}$\times$ & GraphBolt \\
Dynamic BFS, web-Google & VeloGraphX vs NetworKit & {web:.2f}$\times$ lower latency & VeloGraphX \\
Dynamic BFS, ca-GrQc & VeloGraphX vs NetworKit & {grqc:.2f}$\times$ lower latency for NetworKit & NetworKit \\
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
    a, b = r["conservative"], r["large_graph"]
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
\end{{table}}
"""


def selector_figure() -> str:
    return r"""
\begin{figure*}[t]
  \centering
  \includegraphics[width=0.94\textwidth]{figures/selector-crossover.pdf}
  \caption{Repair/recompute crossover and selector quality under the frozen nine-regime harness. Top: median paired incremental/full answer-ready latency ratio; values below 1 favor repair and values above 1 favor full recomputation. Bottom: current adaptive mean and p95 oracle-relative regret.}
  \Description{Two-panel chart across nine graph/update regimes showing the repair/full crossover and the current selector's mean and p95 regret, including the large web-Google tail.}
  \label{fig:selector-crossover}
\end{figure*}
"""


def triangle_figure() -> str:
    return r"""
\begin{figure}[t]
  \centering
  \includegraphics[width=\linewidth]{figures/triangle-exact-reference.pdf}
  \caption{Exact dynamic triangle answer-ready latency against the pinned GoldenCounter exact reference on facebook-combined. All 15 paired executions are exact.}
  \Description{Bar chart comparing VeloGraphX and GoldenCounter exact answer-ready latency at one, five, and ten percent insertion batches.}
  \label{fig:triangle-reference}
\end{figure}
"""


def insert_before_heading(body: str, headings: tuple[str, ...], display: str) -> str:
    for heading in headings:
        pattern = re.compile(rf"(?m)^{re.escape(heading)}$")
        matches = list(pattern.finditer(body))
        if len(matches) == 1:
            m = matches[0]
            return body[:m.start()] + display.strip() + "\n\n" + body[m.start():]
        if len(matches) > 1:
            raise RuntimeError(f"display anchor matched more than once: {heading!r}")
    raise RuntimeError(f"none of the display anchors matched: {headings!r}")


def inject_displays(body: str, results: dict, derived: dict) -> str:
    body = insert_before_heading(body, ("# System design",), system_overview_figure())
    body = insert_before_heading(body, ("## Exact deletion repair",), design_contract_table())
    body = insert_before_heading(body, ("## Reproducibility discipline",), selector_workload_table())
    body = insert_before_heading(
        body,
        ("## Dynamic BFS versus NetworKit",),
        selector_figure() + "\n\n" + policy_baseline_table(derived),
    )
    body = insert_before_heading(
        body,
        ("## Static BFS and weighted SSSP context", "## Static BFS and SSSP versus GAP and LAGraph"),
        external_baseline_table(results),
    )
    body = insert_before_heading(body, ("## Large-graph storage maintenance",), triangle_figure())
    body = insert_before_heading(body, ("## Supporting breadth", "## Supporting breadth and maturity"), storage_table(results))
    orphan = re.search(r"(?m)^#{1,6}\s*$", body)
    if orphan:
        raise RuntimeError(f"orphan Markdown heading after display injection near offset {orphan.start()}")
    return body


def main() -> None:
    text = MANUSCRIPT.read_text(encoding="utf-8")
    results = json.loads(RESULTS.read_text(encoding="utf-8"))
    derived = json.loads(DERIVED.read_text(encoding="utf-8"))
    BUILD.mkdir(parents=True, exist_ok=True)

    abstract = section_between(text, "## Abstract", "## 1. Introduction")
    body_start = text.index("## 1. Introduction")
    body = normalize_body(text[body_start:])
    body = make_typesetting_friendly(body)
    body = add_citations(body)
    body = inject_displays(body, results, derived)

    (BUILD / "abstract.md").write_text(abstract.strip() + "\n", encoding="utf-8")
    (BUILD / "body.md").write_text(body, encoding="utf-8")
    print(f"Prepared synchronized PVLDB manuscript inputs under {BUILD}")


if __name__ == "__main__":
    main()
