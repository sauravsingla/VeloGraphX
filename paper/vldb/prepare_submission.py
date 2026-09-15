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


def make_typesetting_friendly(body: str) -> str:
    """Make the venue rendering line-break friendly without changing claims.

    The canonical manuscript keeps code-style names and full provenance for
    readability on GitHub. Narrow PVLDB columns benefit from ordinary prose for
    long identifiers and from a few equivalent, shorter venue-facing phrases.
    The machine-readable evidence registry remains the canonical provenance
    source. No result, policy, or scientific conclusion is changed here.
    """
    replacements = {
        "`always_incremental`": "always-incremental",
        "`always_full`": "always-full",
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
        "graph/reachability scale": "graph scale and reachability",
        "external-system conclusions are workload-specific": "external-system results vary by workload",
        "graph/update structure": "graph and update structure",
        "the same broad principle—avoid global work while localized state remains economical—but operate at different layers and timescales": "the same broad principle of avoiding global work while localized state remains economical, but they operate at different layers and timescales",
        "repair-versus-recompute selection": "repair/recompute selection",
        "dependency-driven and sparsity-aware incremental graph processing": "dependency- and sparsity-aware incremental graph processing",
    }
    for source, target in replacements.items():
        body = body.replace(source, target)

    # The PDF need not carry long run/artifact IDs inline: the evidence registry
    # is retained with the submission artifact and is the authoritative mapping.
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

    # Any remaining long numeric provenance identifiers should not be monospace.
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
Dynamic BFS & Shortest-parent invalidation, boundary repair, insertion decrease propagation & Exact full BFS recomputation & Pre-repair selector plus 35\% affected-region fallback bound \\
Selection & Structural preflight plus recent arm-cost history & Direct choice of full execution before repair & Per-batch decision reason, arm ages, predicted costs, decision time \\
Correctness & Maintained exact state & Independent exact reference & Verification outside the timed region for publication runs \\
\bottomrule
\end{tabular}
\end{table*}
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
  \includegraphics[width=0.94\textwidth]{figures/selector-regret.pdf}
  \caption{Current publication selector across three graph families and three update regimes per graph. Top: mean and p95 oracle-relative regret. Bottom: wrong-arm choices, explicit pre-repair full choices, and internal fallback. The large web-Google tail is intentionally retained; internal fallback is zero in all nine regimes.}
  \Description{Two-panel chart over nine graph/update regimes. The top panel shows mean and p95 oracle-relative regret; the lower panel shows wrong-arm, explicit full-choice, and internal-fallback percentages. Most regimes have low regret and zero fallback, while the largest web-Google regime has the largest regret tail and a one-third wrong-arm rate.}
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
        ("## Exact deletion repair", design_contract_table() + "\n## Exact deletion repair"),
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
    body = make_typesetting_friendly(body)
    body = add_citations(body)
    body = inject_displays(body, results)

    (BUILD / "abstract.md").write_text(abstract + "\n", encoding="utf-8")
    (BUILD / "body.md").write_text(body, encoding="utf-8")

    print(f"Prepared cited manuscript and evidence displays under {BUILD}")


if __name__ == "__main__":
    main()
