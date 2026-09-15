# VeloGraphX: Adaptive Exact Analytics for Evolving Graphs

> Working manuscript. Quantitative claims in this file are restricted to the evidence selected in `results-ledger.md` and `../docs/paper-evidence-index.md`.

## Abstract

Graph analytics systems increasingly operate on graphs that change continuously, yet the execution strategy for maintaining an exact analytical result is often fixed in advance: either recompute the result after updates or maintain it incrementally. Neither strategy dominates across update regimes. Localized repair can avoid most graph work when changes have limited impact, while recomputation becomes preferable when the affected state grows or repair overhead accumulates.

We present **VeloGraphX**, a C++20 system for exact analytics on evolving graphs that exposes both localized repair and full recomputation over a mutable graph substrate and makes the execution choice at runtime. The system combines segmented CSR storage, packed mutable deltas, sparse row patches, forward/reverse adjacency, exact incremental algorithms, and an adaptive execution layer. Its design treats fallback to recomputation as part of the correctness and performance contract rather than as a failure of incremental execution.

Our evaluation is organized around the crossover between the two exact strategies and uses pinned datasets, repeated measurements, explicit timing envelopes, exactness checks, and retained artifacts. The benchmark record deliberately retains losses as well as wins. In paired dynamic-BFS experiments, VeloGraphX is about 1.38× faster than NetworKit on the evaluated `web-Google` workload, while NetworKit is about 1.35× faster on `ca-GrQc`, with all 30 paired executions exact. For exact dynamic triangle counting on `facebook-combined`, VeloGraphX reaches the same exact post-update answer as a pinned published GoldenCounter reference with 40.95×, 6.94×, and 3.48× lower median answer-ready latency at 1%, 5%, and 10% insertion batches. On `com-Orkut` with 234.4 million directed arcs, a scale-aware bounded storage policy improves maintenance-amortized throughput by 2.25× and reduces consolidation time by 59.6% at a 6.6% peak-RSS cost. These results support a narrower systems conclusion than universal superiority: exact dynamic analytics benefits from treating repair and recomputation as competing execution modes whose preferred choice changes with workload regime.

## 1. Introduction

Large graphs are rarely static. Relationship, transaction, knowledge, infrastructure, and dependency graphs receive continuous insertions and deletions while applications continue to ask graph-analytic questions. A system that maintains exact answers under those changes faces a basic execution decision after each update batch: should it repair only the state that may have changed, or should it recompute the result over the updated graph?

The usual framing makes one of these choices architectural. Static engines optimize full recomputation. Dynamic engines emphasize incremental maintenance. In practice, this boundary is workload-dependent. Small or structurally local updates can make incremental repair dramatically cheaper than a graph-wide traversal. Larger updates, destructive changes, or broad dependency cascades can reverse the ordering: the work needed to discover and repair affected state can approach or exceed the cost of a fresh computation. A system that hard-codes either choice therefore leaves performance on the table in the regime where the other choice is preferable.

VeloGraphX is built around the observation that **repair and recomputation are two exact physical execution strategies for the same logical analytic result**. The system keeps both strategies available behind a common mutable graph representation. Incremental algorithms identify and repair affected state; conservative fallbacks preserve exactness; and an adaptive execution layer uses observable workload state and measured costs to choose how to execute subsequent batches.

This framing affects the storage layer as well. Rebuilding a canonical CSR after every update defeats much of the benefit of localized analytics, but indefinitely accumulating mutable overlays makes traversal and maintenance increasingly expensive. VeloGraphX therefore separates a compact CSR base from packed mutable deltas and sparse row patches, and performs canonical consolidation explicitly under bounded policies. The same principle appears at both levels: defer global work while it is economical to maintain local state, then switch to a global operation when the local path ceases to be attractive.

The implementation supports exact maintained BFS/unweighted SSSP, weighted SSSP with conservative fallback, connected components, triangle counting, k-core, and PageRank-related maintenance. The paper uses BFS as the primary vehicle for studying adaptive repair versus recomputation because it exposes both insertion- and deletion-induced dependency changes and admits a direct exact recomputation oracle. Additional algorithms and storage experiments establish that the system mechanisms are not specific to one BFS benchmark.

This paper makes four contributions:

1. **A mutable graph substrate for repeated exact analytics.** VeloGraphX combines segmented CSR, packed delta storage, sparse row patches, forward/reverse adjacency, and explicit consolidation so updates do not require rebuilding canonical CSR after every batch.
2. **Exact maintained analytics with recomputation as a first-class fallback.** The dynamic algorithms repair localized affected state when possible while preserving an exact result through explicit fallback when repair is unsafe or uneconomical.
3. **Adaptive repair-versus-recompute execution.** VeloGraphX exposes both exact execution modes to a policy layer rather than assuming that one mode dominates across workloads.
4. **A reproducible characterization of the crossover.** The evaluation uses checksum-pinned data, repeated raw measurements, exactness gates, explicit timing semantics, competitor revision pins, and negative-result retention. The goal is to establish when each strategy or system wins, not to construct a win-only benchmark table.

The strongest claim of the paper is therefore not that VeloGraphX is universally the fastest graph system. It is that the preferred exact execution mode changes materially across dynamic regimes, and that system architecture should make this choice explicit and adaptable.

## 2. Problem and execution model

Let a graph state after batch `t` be `G_t`, obtained by applying update batch `U_t` to `G_{t-1}`. For an exact graph analytic `F`, the system must produce a result `R_t = F(G_t)` after every batch. We consider two physical strategies:

- **Full recomputation**, which applies the updates and computes `F(G_t)` from scratch.
- **Localized repair**, which uses `R_{t-1}`, the update batch, graph structure, and algorithm-specific dependency information to identify affected state and transform the previous exact result into `R_t`.

Both strategies have the same semantic target. Their costs differ with graph size, update size, reachability, the location of destructive changes, affected-region size, mutable-storage state, and recent observed execution cost.

For a batch `U_t`, an offline oracle can be defined as the minimum measured answer-ready cost among the exact candidate strategies under the same graph and update stream. An online selector cannot observe this oracle before executing the batch; it must use pre-execution features and historical observations. We evaluate policy quality relative to this per-regime or per-batch exact oracle while separately reporting the selector's own decision cost and any one-time setup cost.

### 2.1 Correctness contract

Exactness is non-negotiable. Every maintained result used in a performance comparison is checked against an independently computed exact reference outside the timed region. A policy is free to choose recomputation, and an incremental path is free to fall back internally, without weakening the semantic contract.

### 2.2 Timing contract

We distinguish kernel/answer-ready latency from process-wall costs that include loading or representation construction. Comparisons use only compatible timing envelopes. In particular, timings from different GitHub-hosted runs are never merged into a synthetic league table. Same-run and paired hosted measurements support scoped relative claims on the evaluated runner; they do not establish universal peak throughput or many-core scalability.

## 3. System design

### 3.1 Mutable graph substrate

The storage design separates a compact base representation from mutable state. The base uses segmented CSR with sorted adjacency. Update batches are represented in packed mutable delta arenas, and frequently touched rows can be represented as sparse compact patches. Forward and reverse adjacency support algorithms that need predecessor information. Overlay cancellation removes update pairs that negate each other. Canonical CSR consolidation is explicit rather than mandatory after every batch.

This layout targets two competing costs. Immediate whole-graph reconstruction simplifies subsequent traversal but turns every update epoch into O(E) maintenance. Retaining too much mutable state reduces update cost but eventually increases traversal and memory overhead. VeloGraphX therefore treats consolidation as another adaptive maintenance decision under bounded storage and latency policies.

### 3.2 Exact localized repair

For BFS, the maintained state includes distances and dependency information sufficient to propagate insertion-induced decreases and discover deletion-affected regions. Additions can propagate shorter paths from changed endpoints. Deletions first determine whether removed edges supported shortest-path dependencies; affected vertices are invalidated and repaired from still-valid predecessors. If the affected region exceeds a configured budget, the implementation switches to full recomputation rather than continuing an increasingly global repair.

This fallback is an intended execution path. It separates semantic correctness from the performance question of whether localized work remains worthwhile.

Other maintained analytics follow the same systems principle with algorithm-specific update logic. Their presence demonstrates breadth, but the paper does not infer a single universal crossover threshold across algorithms.

### 3.3 Adaptive execution

The adaptive layer compares exact execution choices rather than approximate answers. Candidate policies include always-full recomputation, always-incremental execution, a simple update-density threshold, and the workload-aware adaptive policy. Observable signals include update fraction, reachable-state characteristics, shallow dependency deletions, previous affected work, graph scale, and recent measured execution costs. The implementation also separates selector decision cost from one-time setup work.

Large-graph behavior is deliberately conservative. The selector can prefer direct recomputation when preflight state indicates that paying for repair discovery followed by an internal fallback would duplicate work. Conversely, when the evidence does not justify a global operation, the policy can probe or retain the localized path and update its cost history.

The current submitted-manuscript selector must be identified by an audited run/artifact in `results-ledger.md`. Historical selector-development campaigns are useful for explaining design evolution but are not silently substituted for the current implementation.

## 4. Experimental methodology

### 4.1 Questions

The evaluation answers five primary questions.

**RQ1 — Correctness and crossover.** Do exact localized repair and full recomputation exchange the performance lead as update impact changes, and can the adaptive policy track the preferable mode without sacrificing exactness?

**RQ2 — External dynamic BFS comparison.** How does VeloGraphX compare with established dynamic graph systems under same-run or paired semantics, and do conclusions change by graph family?

**RQ3 — Static execution context.** Is the underlying engine competitive with optimized static graph libraries, and where does it lose?

**RQ4 — Algorithmic breadth.** Do exact dynamic mechanisms provide meaningful benefits beyond BFS, under a semantically fair published reference comparison?

**RQ5 — Storage maintenance.** Does delaying whole-graph canonicalization reduce steady-state maintenance cost at large graph scale, and what memory trade-off does it impose?

### 4.2 Reproducibility discipline

Datasets are checksum-pinned and preprocessing is recorded. External systems are pinned to immutable revisions where possible. Raw repetitions are retained before aggregation. Dynamic workloads verify exactness after every measured execution. The benchmark record reports competitor wins and negative results. Machine-readable artifacts record environment and timing semantics.

Hosted CI is used as reproducible comparative evidence when alternatives execute within the same documented envelope. We do not use it to claim stable 8/16/32-core scaling, true multi-socket NUMA behavior, hardware-counter superiority, NVMe performance, or universal peak throughput.

### 4.3 Statistical reporting

The manuscript reports medians for repeated latency measurements and dispersion where raw samples permit it. Policy experiments additionally report oracle-relative regret, tail regret, wrong-arm rate or equivalent decision diagnostics, full-recomputation frequency, and selector overhead. Averages over heterogeneous graph families are not used to conceal per-dataset reversals.

## 5. Evaluation

### 5.1 Exact repair and recomputation have a crossover

The central experiment compares the same exact result under always-incremental, always-full, simple-threshold, and adaptive policies. The final manuscript number for the current selector is inserted only after its publication-selector artifact is audited. Historical evidence already establishes the qualitative crossover: exact BFS repair is faster than full recomputation at small update regimes, whereas full recomputation becomes faster once repair scope is sufficiently large. The submitted result should report this transition together with exactness and oracle-relative policy quality, not only a best-case speedup.

This result supports the paper's primary thesis: update density alone is not a universal decision rule, but neither is unconditional incremental maintenance.

### 5.2 Dynamic BFS versus NetworKit

The accepted NetworKit campaign uses NetworKit 11.2.1, one thread, two graph families, three fixed roots per dataset, and five paired repetitions per root. All 30 paired executions are exact. On `web-Google`, the mean VeloGraphX/NetworKit paired latency ratio is about 0.73, corresponding to approximately 1.38× lower latency for VeloGraphX on that evaluated workload. On `ca-GrQc`, the ratio is about 1.35, so NetworKit is approximately 1.35× faster.

The reversal is important. It demonstrates that external-system conclusions are workload-specific and provides a stronger result than a win-only comparison. The paper does not generalize these two datasets to universal superiority.

### 5.3 Static BFS and SSSP versus GAP and LAGraph

A same-run hosted campaign compares VeloGraphX with GAP Benchmark Suite and LAGraph/SuiteSparse:GraphBLAS at 1, 2, and 4 threads, with five repetitions per configuration and independent correctness checks. VeloGraphX is fastest in the evaluated BFS cases, measuring 1.60×–2.04× faster than GAP and 9.4×–11.8× faster than LAGraph. Weighted SSSP gives the opposite lesson: GAP is fastest, while VeloGraphX is 2.6×–3.0× faster than LAGraph but 7.0×–8.5× slower than GAP.

These results establish that the engine can be competitive on its primary traversal while preventing the paper from implying that every kernel is uniformly optimized.

### 5.4 Exact dynamic triangles versus a published reference

We compare VeloGraphX with the exact `GoldenCounter` implementation distributed with published SIGMOD 2021 source code, pinned to an immutable revision. Because `GoldenCounter::insert_edge` does not by itself make the exact global triangle answer available, the semantically comparable baseline is insertion plus its subsequent exact `triangle_count()` query.

On normalized `facebook-combined` (4,039 vertices and 88,234 undirected edges), all 15 paired comparisons at 1%, 5%, and 10% insertion batches produce identical exact counts. Median VeloGraphX answer-ready latency is 1.066 ms, 6.782 ms, and 15.495 ms, compared with 43.657 ms, 47.095 ms, and 53.931 ms for the published exact reference. This corresponds to 40.95×, 6.94×, and 3.48× lower answer-ready latency, respectively.

This is evidence about the pinned exact reference component, not a claim that VeloGraphX outperforms the paper's approximate sliding-window algorithm, whose semantics differ.

### 5.5 Large-graph storage maintenance

The accepted canonicalization A/B uses SNAP `com-Orkut`, with 3,072,441 vertices and 234,370,166 directed arcs in the dynamic representation, over 60 mutation/maintenance epochs. It compares a conservative 1.25× owned-storage envelope with a bounded large-graph 1.50× policy while preserving exact edge counts and consolidation digests.

The larger bounded envelope reduces consolidations from 15 to 6 and total consolidation time from 386.573 s to 156.128 s, a 59.6% reduction. Maintenance-amortized throughput increases from 19,135 to 43,062 operations/s, or 2.25×. The trade-off is higher process peak RSS: 8,016,740 KiB versus 7,517,892 KiB, approximately 6.6% higher.

This experiment shows that whole-graph canonicalization can dominate maintenance at 100M+-arc scale and that a bounded delay can materially improve amortized cost. It does not establish that the 1.50× threshold is universally optimal.

### 5.6 Supporting multicore and compression evidence

Hosted engineering campaigns show 4-thread speedups of 2.74× for BFS, 2.50× for connected components, and 2.24× for triangle counting, and compression ratios of approximately 3.25×–3.78× with a documented traversal trade-off. These observations support implementation maturity but are secondary to the adaptive dynamic-execution story. We deliberately avoid extrapolating them to many-core or NUMA claims.

## 6. Discussion

### 6.1 Why negative results strengthen the systems claim

The benchmark record contains three useful reversals: full recomputation wins once repair becomes too broad; NetworKit wins the evaluated `ca-GrQc` dynamic-BFS workload; and GAP substantially wins the evaluated weighted-SSSP workload. These are not exceptions to remove from the evaluation. They are evidence for the central proposition that execution choices and system rankings depend on workload regime.

### 6.2 Repair discovery can itself be wasted work

A selector that invokes incremental repair and only later discovers that the affected region is too large may pay twice: once to discover or partially process the repair region and again for full recomputation. This motivates pre-execution signals and selector-owned recomputation. Current publication-policy experiments therefore track internal fallback and wrong-arm behavior rather than reporting mean latency alone.

### 6.3 Storage and algorithm adaptation are related but distinct

The storage policy and analytical selector embody the same broad principle—avoid global work while localized state remains economical—but operate at different layers and timescales. The paper should not collapse them into one learned policy. Storage consolidation is a bounded maintenance decision; analytic repair-versus-recompute is an answer-ready execution decision.

## 7. Limitations

The primary comparative evidence is hosted and therefore intentionally scoped. The paper does not claim stable many-core scaling, multi-socket NUMA behavior, hardware-counter advantages, NVMe/out-of-core superiority, or universal peak throughput. NetworKit evidence covers one thread and two graph families. The accepted RisGraph and NetworKit campaigns were executed separately, so their absolute latencies are not combined into a three-system ranking unless a unified same-machine campaign is separately audited.

The system supports multiple maintained analytics, but the adaptive policy is studied most deeply for BFS. Algorithm-specific performance conclusions require algorithm-specific evidence. Some destructive weighted-SSSP updates conservatively recompute. The Python API and some 0.x interfaces remain subject to change.

Finally, historical selector-development experiments changed multiple mechanisms across iterations. They motivate the final design but do not constitute a clean cumulative component ablation. Causal claims about individual selector mechanisms require an explicit feature-switched ablation under one frozen harness.

## 8. Related work

The final venue manuscript should position VeloGraphX against four bodies of work: (1) static high-performance graph analytics such as GAP and GraphBLAS/LAGraph; (2) dynamic and streaming graph systems, including systems such as RisGraph and dynamic primitives in NetworKit; (3) incremental computation and dynamic graph algorithms; and (4) mutable graph storage and dynamic graph representations. Related-work text should be completed from primary papers and the exact versions used in the experimental artifacts rather than inferred from benchmark names alone.

## 9. Conclusion

VeloGraphX treats exact localized repair and full recomputation as alternative physical execution strategies for the same evolving-graph analytics problem. A mutable graph substrate makes repeated updates practical, exact incremental algorithms expose localized work, and an adaptive layer can avoid committing the system to one execution mode across all regimes. The evaluation deliberately includes regimes where recomputation and external systems win, because those reversals are central to the result: dynamic graph execution has a crossover.

The resulting design lesson is simple but consequential for graph systems: **incremental maintenance should be a selectable execution strategy, not an unconditional architectural assumption.**
