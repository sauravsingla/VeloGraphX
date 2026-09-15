# VeloGraphX: Adaptive Exact Analytics for Evolving Graphs

> Working manuscript. Quantitative claims in this file are restricted to `results-ledger.md` and `../docs/paper-evidence-index.md`.

## Abstract

Graph analytics systems increasingly operate on graphs that change continuously, yet the execution strategy for maintaining an exact analytical result is often fixed in advance: either recompute the result after updates or maintain it incrementally. Neither strategy dominates across update regimes. Localized repair can avoid most graph work when changes have limited impact, while recomputation becomes preferable when affected state grows or repair-discovery overhead accumulates.

We present **VeloGraphX**, a C++20 system for exact analytics on evolving graphs that exposes localized repair and full recomputation as competing physical execution strategies over a common mutable graph substrate. VeloGraphX combines segmented CSR storage, packed mutable deltas, sparse row patches, forward/reverse adjacency, exact maintained analytics, and a pre-repair selector that uses structural state and observed cost to choose how to execute subsequent batches.

The evaluation uses checksum-pinned datasets, repeated measurements, explicit timing envelopes, exactness checks, retained artifacts, and negative-result retention. On a three-graph current-policy validation covering nine regimes and 1,610 adaptive batch samples, every output is exact; the selector records 3.94% mean oracle regret across regimes, 2.31% sample-weighted regret, a 1.74% sample-weighted wrong-arm rate, zero internal full fallbacks, and about 0.286 µs sample-weighted decision cost. The tail is not hidden: the largest evaluated `web-Google` regime reaches 17.48% mean regret and 54.42% p95 regret. In paired dynamic-BFS experiments, VeloGraphX is about 1.38× faster than NetworKit on the evaluated `web-Google` workload, while NetworKit is about 1.35× faster on `ca-GrQc`, with all 30 paired executions exact. Exact dynamic triangle counting reaches the same post-update answer as a pinned GoldenCounter reference with 40.95×, 6.94×, and 3.48× lower median answer-ready latency at 1%, 5%, and 10% insertion batches. On `com-Orkut` with 234.4 million directed arcs, a wider bounded storage envelope improves maintenance-amortized throughput by 2.25× and reduces consolidation time by 59.6% at a 6.6% peak-RSS cost.

These results support a narrower conclusion than universal system superiority: exact dynamic analytics benefits from treating repair and recomputation as selectable execution modes whose preferred choice changes with graph and update regime.

## 1. Introduction

Large graphs are rarely static. Relationship, transaction, knowledge, infrastructure, and dependency graphs receive continuous insertions and deletions while applications continue to ask graph-analytic questions. After each update batch, an exact system faces a basic execution decision: should it repair only the state that may have changed, or should it recompute the result over the updated graph?

The usual framing makes one choice architectural. Static engines optimize full recomputation. Dynamic engines emphasize incremental maintenance. In practice, the boundary is workload-dependent. Small or structurally local updates can make localized repair much cheaper than a graph-wide traversal. Larger updates, destructive changes, or broad dependency cascades can reverse the ordering: the work needed to discover and repair affected state can approach or exceed the cost of a fresh computation. A system that hard-codes either strategy can therefore pay unnecessary work in regimes where the other strategy is preferable.

VeloGraphX is built around the observation that **repair and recomputation are two exact physical execution strategies for the same logical analytic result**. The system keeps both strategies available behind a common mutable graph representation. Incremental algorithms identify and repair affected state; conservative fallbacks preserve exactness; and a pre-repair selector uses observable workload state and measured costs to choose how to execute subsequent batches.

The same tension appears in storage. Rebuilding a canonical CSR after every update defeats much of the benefit of localized analytics, but indefinitely accumulating mutable overlays makes traversal and maintenance increasingly expensive. VeloGraphX separates a compact CSR base from packed mutable deltas and sparse row patches and performs canonical consolidation explicitly under bounded policies. Both layers therefore follow the same systems principle: avoid global work while local state remains economical, then switch to a global operation when the local path stops being attractive.

VeloGraphX supports exact maintained BFS/unweighted SSSP, weighted SSSP with conservative fallback, connected components, triangle counting, k-core, and PageRank-related workflows. We use BFS as the primary vehicle for studying adaptive repair versus recomputation because it exposes insertion- and deletion-induced dependency changes and admits a direct exact recomputation oracle. Additional algorithms and storage experiments establish that the system mechanisms are not specific to a single BFS benchmark.

The paper makes four contributions:

1. **A mutable graph substrate for repeated exact analytics.** Segmented CSR, packed deltas, sparse row patches, forward/reverse adjacency, and explicit consolidation avoid mandatory canonical reconstruction after every batch.
2. **Exact maintained analytics with recomputation as a first-class execution path.** Dynamic algorithms repair localized state where economical while preserving exactness through explicit recomputation when appropriate.
3. **Pre-repair repair-versus-recompute selection.** VeloGraphX exposes both exact modes to a policy layer so it can avoid both unnecessary global work and repair-discovery work that would subsequently fall back to full execution.
4. **A reproducible characterization of the crossover.** The evaluation uses pinned data and revisions, repeated raw measurements, exactness gates, explicit timing semantics, current-policy oracle metrics, external baselines, and negative-result retention.

The strongest claim is therefore not that VeloGraphX is universally the fastest graph system. It is that the preferred exact execution mode changes materially across dynamic regimes, and system architecture should make this choice explicit and adaptable.

## 2. Problem and execution model

Let graph state `G_t` result from applying update batch `U_t` to `G_{t-1}`. For an exact graph analytic `F`, the system must produce `R_t = F(G_t)` after every batch. We consider two physical strategies:

- **Full recomputation**, which applies `U_t` and computes `F(G_t)` from scratch.
- **Localized repair**, which uses `R_{t-1}`, `U_t`, graph structure, and algorithm-specific dependency information to transform the prior exact result into `R_t`.

Both target the same semantics. Their costs differ with graph size, update size, reachability, destructive-change location, affected-region size, mutable-storage state, and recent observed execution cost.

For each batch, an offline oracle is the minimum measured answer-ready cost among the exact incremental and full alternatives under the same graph and update stream. An online selector cannot observe this oracle before executing the batch; it must choose from pre-execution features and historical observations. We therefore evaluate selector quality by oracle-relative regret while separately recording wrong-arm choices, decision cost, explicit full choices, and internal fallback.

### 2.1 Correctness contract

Exactness is non-negotiable. Every maintained result used in performance comparison is checked against an independently computed exact reference outside the timed region. A policy may choose recomputation and an incremental implementation may conservatively fall back without weakening semantics.

### 2.2 Timing contract

We distinguish answer-ready latency from process-wall costs that include loading or representation construction. Comparisons use only compatible timing envelopes. Absolute timings from separate GitHub-hosted campaigns are not merged into a synthetic ranking. Same-run and paired hosted measurements support scoped relative claims on the evaluated runner; they do not establish universal peak throughput or many-core scalability.

## 3. System design

### 3.1 Mutable graph substrate

The storage design separates a compact base representation from mutable state. The base uses segmented CSR with sorted adjacency. Update batches are represented in packed mutable delta arenas, and frequently touched rows can become sparse compact patches. Forward and reverse adjacency support predecessor-dependent algorithms. Overlay cancellation removes update pairs that negate each other. Canonical CSR consolidation is explicit rather than mandatory after every batch.

This layout balances two costs. Immediate whole-graph reconstruction simplifies subsequent traversal but turns every update epoch into O(E) maintenance. Retaining too much mutable state reduces update cost but eventually increases traversal and memory overhead. VeloGraphX therefore treats consolidation as a bounded maintenance decision rather than an unconditional update step.

### 3.2 Exact localized repair

For BFS, maintained state includes distances and dependency information sufficient to propagate insertion-induced decreases and discover deletion-affected regions. Additions can propagate shorter paths from changed endpoints. Deletions determine whether removed edges support shortest-path dependencies; affected vertices are invalidated and repaired from still-valid predecessors. The general implementation retains a conservative full-recomputation fallback when repair becomes sufficiently broad.

That fallback is an intended execution path. It separates semantic correctness from the performance question of whether localized work remains worthwhile.

### 3.3 Pre-repair adaptive execution

The publication harness compares `always_incremental`, `always_full`, a simple update-density threshold, a history-cost baseline, and the current `publication-preflight-v1` selector. Observable signals include update fraction, reachable-state characteristics, shallow dependency deletions, previous affected work, graph scale, and recent measured execution costs. The selector records its own decision cost separately from execution cost.

For large graphs, the current policy uses the initial exact BFS as an already-measured full-cost baseline and probes the missing incremental arm instead of redundantly executing full recomputation again. It can select full execution directly when preflight evidence is strong, thereby avoiding the undesirable path of paying for repair discovery only to fall back internally to a full traversal.

Historical selector versions are retained for development provenance, but the manuscript uses only audited current-policy artifacts for headline quantitative claims.

## 4. Prior art and novelty boundary

VeloGraphX does not claim that switching between incremental and static execution is new. GraphIn explicitly introduced a property-based dual-path execution model. Nor is cost/history-based selection new: Bok et al. predict incremental processing cost from prior execution history and select between incremental and static processing. GraphBolt and DZiG establish dependency-driven and sparsity-aware incremental graph processing, while RisGraph and Layph address low-latency evolving-graph execution and broad change propagation.

The contribution claimed here is narrower and system-level: VeloGraphX exposes localized repair and full recomputation as exact physical alternatives over one mutable substrate, performs the choice before expensive repair work when possible, records the double-work/fallback consequences of bad choices, and evaluates the crossover under an artifact-backed contract that retains both selector and competitor losses. The bibliography and claim-by-claim novelty notes are maintained in `references.bib` and `related-work-notes.md`.

## 5. Experimental methodology

### 5.1 Research questions

**RQ1 — Correctness and crossover.** Do exact localized repair and full recomputation exchange the performance lead as update impact changes, and can the current pre-repair policy track the preferable arm without sacrificing exactness?

**RQ2 — External dynamic BFS comparison.** How does VeloGraphX compare with established dynamic graph systems under paired/same-run semantics, and do conclusions change by graph family?

**RQ3 — Static execution context.** Is the underlying engine competitive with optimized static graph libraries, and where does it lose?

**RQ4 — Algorithmic breadth.** Do exact dynamic mechanisms provide meaningful benefits beyond BFS under a semantically fair published-reference comparison?

**RQ5 — Storage maintenance.** Does delaying whole-graph canonicalization reduce maintenance cost at large graph scale, and what memory trade-off does it impose?

### 5.2 Reproducibility discipline

Datasets are checksum-pinned and preprocessing is recorded. External systems are pinned to immutable revisions where possible. Raw repetitions are retained before aggregation. Dynamic workloads verify exactness after every measured execution. The benchmark record retains competitor wins and negative results. Machine-readable artifacts record environment and timing semantics.

Hosted CI is used as reproducible comparative evidence when alternatives execute under the same documented envelope. We do not use hosted evidence to claim stable 8/16/32-core scaling, true multi-socket NUMA behavior, hardware-counter superiority, NVMe performance, or universal peak throughput.

### 5.3 Statistical reporting

We report medians for repeated latency measurements and dispersion where raw samples permit it. Policy experiments additionally report oracle-relative regret, tail regret, wrong-arm rate, full-recomputation frequency, internal fallback, and selector overhead. Averages do not replace per-dataset or per-regime results when winner reversals occur.

## 6. Evaluation

### 6.1 Current selector: exact, low average regret, visible tail

The primary current-policy campaign is GitHub Actions run `34929398888` with retained artifact `10381490811`. It evaluates checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google`, one fixed root per graph, three batch regimes per graph, five repetitions per regime, and one thread. The historical graph/root program is reused for cross-dataset current-policy validation; we do not describe it as a newly unseen holdout.

Across **nine regimes and 1,610 adaptive batch samples**, all policy outputs match exact BFS. The current selector records **3.939% mean oracle regret across regimes**, **2.309% sample-weighted mean regret**, a **1.739% sample-weighted wrong-arm rate**, **zero internal full fallbacks**, and about **0.286 µs sample-weighted decision cost**. The obsolete redundant one-sided-full decision is never taken.

The result is not uniformly near-oracle. On `soc-Epinions1`, all three regimes have zero wrong-arm selections and mean regret between 0.86% and 1.10%. On `ca-GrQc`, the selector moves from all-incremental at batch 96 to all-full at batch 1,536; the middle batch has an 18.9% wrong-arm rate but 2.83% mean regret. The largest `web-Google` regime, batch 24,576, is the principal tail weakness: mean regret is **17.477%**, p95 regret **54.424%**, and wrong-arm rate **33.3%** over 15 batch samples. This regime is retained in the primary result rather than tuned away.

A separate focused `web-Google` regression run (`34928935983`, artifact `10381480310`) validates the current policy across four other batch sizes. It is 100% exact, records 2.466% mean regret across regimes, 15.671% worst-regime p95 regret, 27.717% maximum single-batch regret, zero internal fallbacks, and zero redundant one-sided-full decisions. We use this as regression evidence rather than as a cross-dataset claim.

Together these experiments support the central thesis without claiming oracle optimality: exact repair and recomputation have a real crossover, and a pre-repair policy can capture much of that benefit while still exposing a measurable tail where the decision remains difficult.

### 6.2 Dynamic BFS versus NetworKit

The accepted NetworKit campaign uses NetworKit 11.2.1, one thread, two graph families, three fixed roots per dataset, and five paired repetitions per root. All 30 paired executions are exact. On `web-Google`, the mean VeloGraphX/NetworKit paired latency ratio is about 0.73, corresponding to approximately **1.38× lower latency for VeloGraphX** on that evaluated workload. On `ca-GrQc`, the ratio is about 1.35, so **NetworKit is approximately 1.35× faster**.

The reversal is important: external-system conclusions are workload-specific. We do not generalize these two datasets to universal superiority.

### 6.3 Static BFS and SSSP versus GAP and LAGraph

A same-run hosted campaign compares VeloGraphX with GAP Benchmark Suite and LAGraph/SuiteSparse:GraphBLAS at 1, 2, and 4 threads, with five repetitions per configuration and independent correctness checks. VeloGraphX is fastest in the evaluated BFS cases, measuring **1.60×–2.04× faster than GAP** and **9.4×–11.8× faster than LAGraph**. Weighted SSSP gives the opposite lesson: **GAP is fastest**; VeloGraphX is 2.6×–3.0× faster than LAGraph but 7.0×–8.5× slower than GAP.

These results show that the engine is competitive on its primary traversal while preventing the manuscript from implying that every kernel is uniformly optimized.

### 6.4 Exact dynamic triangles versus a published exact reference

We compare VeloGraphX with the exact `GoldenCounter` implementation distributed with published SIGMOD 2021 source code, pinned to an immutable revision. Because `GoldenCounter::insert_edge` does not itself make the exact global triangle answer available, the comparable baseline is insertion plus the subsequent exact `triangle_count()` query.

On normalized `facebook-combined` (4,039 vertices and 88,234 undirected edges), all 15 paired comparisons at 1%, 5%, and 10% insertion batches produce identical exact counts. Median VeloGraphX answer-ready latency is 1.066 ms, 6.782 ms, and 15.495 ms, versus 43.657 ms, 47.095 ms, and 53.931 ms for the published exact reference: **40.95×, 6.94×, and 3.48× lower answer-ready latency**, respectively.

This is evidence about the pinned exact reference component, not a claim that VeloGraphX outperforms the paper's approximate sliding-window algorithm, whose semantics differ.

### 6.5 Large-graph storage maintenance

The accepted canonicalization A/B uses SNAP `com-Orkut`, with 3,072,441 vertices and 234,370,166 directed arcs in the dynamic representation, over 60 mutation/maintenance epochs. It compares a conservative 1.25× owned-storage envelope with a bounded 1.50× large-graph policy while preserving exact edge counts and consolidation digests.

The wider bounded envelope reduces consolidations from 15 to 6 and total consolidation time from 386.573 s to 156.128 s, a **59.6% reduction**. Maintenance-amortized throughput increases from 19,135 to 43,062 operations/s, or **2.25×**. The trade-off is higher peak RSS: 8,016,740 KiB versus 7,517,892 KiB, approximately **6.6% higher**.

This experiment shows that whole-graph canonicalization can dominate maintenance at 100M+-arc scale and that a bounded delay can materially improve amortized cost. It does not establish that the 1.50× threshold is universally optimal.

### 6.6 Supporting breadth and maturity

A fresh three-dataset triangle crossover campaign retains five repetitions at 13 update fractions per dataset and exactness against full recomputation. `facebook-combined` remains incremental-favorable through the largest tested ratio, while `p2p-Gnutella08` and `ca-HepTh` cross into full-recompute-favorable regimes at sufficiently large updates. This supports the broader graph-dependent crossover story but is secondary to the BFS policy experiment.

Hosted engineering campaigns also show 4-thread speedups of 2.74× for BFS, 2.50× for connected components, and 2.24× for triangle counting, and compression ratios of approximately 3.25×–3.78× with a documented traversal trade-off. These are implementation-maturity results, not many-core claims.

## 7. Discussion

### 7.1 Negative results are part of the result

The benchmark record contains several useful reversals: full recomputation wins when repair becomes sufficiently broad; NetworKit wins the evaluated `ca-GrQc` dynamic-BFS workload; GAP substantially wins weighted SSSP; RisGraph remains faster on the documented hosted dynamic-BFS run; and the current selector has a visible large-`web-Google` tail. Removing these cases would weaken rather than strengthen the systems claim, because the paper is about workload-dependent execution choices.

### 7.2 Repair discovery can itself be wasted work

A selector that invokes incremental repair and only later discovers that the affected region is too large may pay twice: once for repair discovery or partial processing and again for full recomputation. This motivates pre-execution signals and selector-owned recomputation. The current publication-policy artifacts show zero internal fallbacks in the evaluated current-policy runs, so the policy is avoiding that specific double-work path there; the remaining error is mostly choosing the slower exact arm directly.

### 7.3 Storage and algorithm adaptation are related but distinct

The storage policy and analytical selector embody the same broad principle—avoid global work while localized state remains economical—but operate at different layers and timescales. We do not collapse them into one learned policy. Storage consolidation is a bounded maintenance decision; repair-versus-recompute selection is an answer-ready execution decision.

## 8. Limitations

The primary comparative evidence is hosted and therefore intentionally scoped. The paper does not claim stable many-core scaling, multi-socket NUMA behavior, hardware-counter advantages, NVMe/out-of-core superiority, or universal peak throughput. NetworKit evidence covers one thread and two graph families. The accepted RisGraph and NetworKit campaigns were executed separately, so their absolute latencies are not combined into a three-system ranking unless the unified same-machine campaign is separately audited.

The current policy's three-graph validation uses a historical fixed graph/root program rather than a newly preregistered unseen holdout. Its average regret is low, but the largest `web-Google` regime has a material tail; the paper therefore does not claim uniform near-oracle behavior. The system supports multiple maintained analytics, but adaptive selection is studied most deeply for BFS. Some destructive weighted-SSSP updates conservatively recompute. Python and other 0.x APIs may evolve.

Historical selector-development experiments changed multiple mechanisms across iterations. They motivate the final design but are not a clean cumulative component ablation. If the manuscript makes causal claims about individual selector mechanisms, it should add explicit feature switches under one frozen harness; otherwise mechanism discussion remains descriptive.

## 9. Related work

The final venue version should position VeloGraphX against static graph analytics and sparse-linear-algebra systems; dynamic and streaming graph systems; incremental-computation and dynamic-graph algorithms; and mutable graph representations. The strongest novelty boundary is explicit: GraphIn already provides dual incremental/static execution, Bok et al. provide history-based incremental/static cost selection, GraphBolt and DZiG provide dependency/sparsity-aware incremental processing, RisGraph provides low-latency evolving-graph processing, and Layph addresses broad change propagation. VeloGraphX builds on this landscape by treating exact repair/recompute choice as a pre-repair physical-plan decision under a common mutable substrate and by measuring the consequences of wrong-arm and fallback behavior.

## 10. Conclusion

VeloGraphX treats exact localized repair and full recomputation as alternative physical strategies for the same evolving-graph analytics problem. A mutable graph substrate makes repeated updates practical, exact maintained algorithms expose localized work, and a pre-repair selector can avoid committing the system to one execution mode across all regimes. The evaluation deliberately retains regimes where recomputation, competitors, or the oracle beat the chosen policy, because those reversals are central to the result: dynamic graph execution has a crossover.

The resulting systems lesson is simple: **incremental maintenance should be a selectable exact execution strategy, not an unconditional architectural assumption.**
