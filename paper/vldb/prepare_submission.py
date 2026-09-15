#!/usr/bin/env python3
"""Prepare the PVLDB build directory from the canonical Markdown manuscript.

The script keeps `paper/manuscript.md` as the source of scientific prose while
allowing the official PVLDB template to compile in CI. It adds venue-facing
citations and evidence displays from the committed result registry, then Pandoc
performs Markdown -> LaTeX conversion.
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

    storage_sentence = (
        "The storage design separates a compact base representation from mutable state."
    )
    storage_replacement = (
        "Prior dynamic graph stores such as GraphOne\\cite{kumar2019graphone} and "
        "Teseo\\cite{deleo2021teseo} already establish hybrid and mutable storage "
        "designs for evolving graphs. VeloGraphX does not claim mutable graph storage "
        "itself as new. " + storage_sentence
    )
    if storage_sentence not in body:
        raise RuntimeError("storage prior-work anchor not found")
    body = body.replace(storage_sentence, storage_replacement, 1)
    return body


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
\caption{{Scoped external-baseline results from retained same-run or paired hosted campaigns. Winners and losses are both retained; absolute times from separate campaigns are not combined.}}
\label{{tab:external-baselines}}
\begin{{tabular}}{{llll}}
\toprule
Workload & Comparison & Evaluated result & Winner \\
\midrule
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


def inject_displays(body: str, results: dict) -> str:
    selector_fig = r"""
\begin{figure*}[t]
  \centering
  \includegraphics[width=0.92\textwidth]{figures/selector-regret.pdf}
  \caption{Current publication selector across three graph families and three update regimes per graph. Mean and p95 oracle-relative regret are shown from the audited current-policy campaign. The large web-Google tail is intentionally retained.}
  \Description{Line chart showing mean and p95 oracle-relative selector regret across nine graph/update regimes. Most regimes have low mean regret, while the largest web-Google regime has the largest p95 tail.}
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

    anchors = [
        ("# Dynamic BFS versus NetworKit", selector_fig + "\n# Dynamic BFS versus NetworKit"),
        ("# Static BFS and SSSP versus GAP and LAGraph", external_baseline_table(results) + "\n# Static BFS and SSSP versus GAP and LAGraph"),
        ("# Large-graph storage maintenance", triangle_fig + "\n# Large-graph storage maintenance"),
        ("# Supporting breadth and maturity", storage_table(results) + "\n# Supporting breadth and maturity"),
    ]
    for anchor, replacement in anchors:
        if anchor not in body:
            raise RuntimeError(f"display anchor not found: {anchor}")
        body = body.replace(anchor, replacement, 1)
    return body


def main() -> None:
    text = MANUSCRIPT.read_text(encoding="utf-8")
    results = json.loads(RESULTS.read_text(encoding="utf-8"))
    BUILD.mkdir(parents=True, exist_ok=True)

    abstract = section_between(text, "## Abstract", "## 1. Introduction")
    body_start = text.index("## 1. Introduction")
    body = normalize_body(text[body_start:])
    body = add_citations(body)
    body = inject_displays(body, results)

    (BUILD / "abstract.md").write_text(abstract + "\n", encoding="utf-8")
    (BUILD / "body.md").write_text(body, encoding="utf-8")

    print(f"Prepared cited manuscript and evidence displays under {BUILD}")


if __name__ == "__main__":
    main()
