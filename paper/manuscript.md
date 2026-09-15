# VeloGraphX: Adaptive Exact Analytics for Evolving Graphs

> Working manuscript. Quantitative claims in this file are restricted to `results-ledger.md` and `../docs/paper-evidence-index.md`.

## Abstract

Graph analytics systems increasingly operate on graphs that change continuously, yet the execution strategy for maintaining an exact analytical result is often fixed in advance: either recompute the result after updates or maintain it incrementally. Neither strategy dominates across update regimes. Localized repair can avoid most graph work when changes have limited impact, while recomputation becomes preferable when affected state grows or repair-discovery overhead accumulates.

We present **VeloGraphX**, a C++20 system for exact analytics on evolving graphs that exposes localized repair and full recomputation as competing physical execution strategies over a common mutable graph substrate. VeloGraphX combines segmented CSR storage, packed mutable deltas, sparse row patches, forward/reverse adjacency, exact maintained analytics, and a pre-repair selector that uses structural state and observed cost to choose how to execute subsequent batches. For dynamic BFS, deletion handling discovers vertices that lose all shortest-path support before the batch is applied, repairs invalidated state from still-valid boundary predecessors, and falls back conservatively when the affected region grows beyond a configured bound. The selector sits outside this repair path so it can choose full recomputation before paying repair-discovery cost.

The evaluation uses checksum-pinned datasets, repeated measurements, explicit timing envelopes, exactness checks, retained artifacts, and negative-result retention. On a three-graph current-policy validation covering nine regimes and 1,610 adaptive batch samples, every output is exact; the selector records 3.94% mean oracle regret across regimes, 2.31% sample-weighted regret, a 1.74% sample-weighted wrong-arm rate, zero internal full fallbacks, and about 0.286 µs sample-weighted decision cost. The tail is not hidden: the largest evaluated `web-Google` regime reaches 17.48% mean regret and 54.42% p95 regret. In paired dynamic-BFS experiments, VeloGraphX is about 1.38× faster than NetworKit on the evaluated `web-Google` workload, while NetworKit is about 1.35× faster on `ca-GrQc`, with all 30 paired executions exact. Exact dynamic triangle counting reaches the same post-update answer as a pinned GoldenCounter reference with 40.95×, 6.94×, and 3.48× lower median answer-ready latency at 1%, 5%, and 10% insertion batches. On `com-Orkut` with 234.4 million directed arcs, a wider bounded storage envelope improves maintenance-amortized throughput by 2.25× and reduces consolidation time by 59.6% at a 6.6% peak-RSS cost.

These results support a narrower conclusion than universal system superiority: exact dynamic analytics benefits from treating repair and recomputation as selectable execution modes whose preferred choice changes with graph and update regime.

## 1. Introduction

Large graphs are rarely static. Relationship, transaction, knowledge, infrastructure, and dependency graphs receive continuous insertions and deletions while applications continue to ask graph-analytic questions. After each update batch, an exact system faces a basic execution decision: should it repair only the state that may have changed, or should it recompute the result over the updated graph?

The usual framing makes one choice architectural. Static engines optimize full recomputation. Dynamic engines emphasize incremental maintenance. In practice, the boundary is workload-dependent. Small or structurally local updates can make localized repair much cheaper than a graph-wide traversal. Larger updates, destructive changes, or broad dependency cascades can reverse the ordering: the work needed to discover and repair affected state can approach or exceed the cost of a fresh computation. A system that hard-codes either strategy can therefore pay unnecessary work in regimes where the other strategy is preferable.

The difficulty is not merely update volume. Two batches with the same number of changed edges can have different consequences because they touch different positions in the dependency structure of the maintained result. An insertion near the frontier of a reachable region may trigger little work, while a deletion close to the BFS root can invalidate a large subtree. Reachability density, prior affected work, graph scale, and recent measured arm costs are therefore potentially more informative than update fraction alone. This makes repair-versus-recompute selection resemble a physical-plan choice rather than a fixed property of an algorithm.

VeloGraphX is built around the observation that **repair and recomputation are two exact physical execution strategies for the same logical analytic result**. The system keeps both strategies available behind a common mutable graph representation. Incremental algorithms identify and repair affected state; conservative fallbacks preserve exactness; and a pre-repair selector uses observable workload state and measured costs to choose how to execute subsequent batches.

This placement matters. A common failure mode in adaptive incremental systems is to begin repair, discover that the affected region is large, and then perform a full computation. Correctness is preserved, but the system pays both discovery/partial-repair work and recomputation. VeloGraphX allows the policy to choose full execution **before** the incremental path begins when preflight evidence is sufficiently strong. Internal fallback remains available as a semantic safety net, but it is not the primary adaptation mechanism.

The same tension appears in storage. Rebuilding a canonical CSR after every update defeats much of the benefit of localized analytics, but indefinitely accumulating mutable overlays makes traversal and maintenance increasingly expensive. VeloGraphX separates a compact CSR base from packed mutable deltas and sparse row patches and performs canonical consolidation explicitly under bounded policies. Both layers therefore follow the same systems principle: avoid global work while local state remains economical, then switch to a global operation when the local path stops being attractive.

VeloGraphX supports exact maintained BFS/unweighted SSSP, weighted SSSP with conservative fallback, connected components, triangle counting, k-core, and PageRank-related workflows. We use BFS as the primary vehicle for studying adaptive repair versus recomputation because it exposes insertion- and deletion-induced dependency changes and admits a direct exact recomputation oracle. Additional algorithms and storage experiments establish that the system mechanisms are not specific to a single BFS benchmark.

The paper makes four contributions:

1. **A mutable graph substrate for repeated exact analytics.** Segmented CSR, packed deltas, sparse row patches, forward/reverse adjacency, and explicit consolidation avoid mandatory canonical reconstruction after every batch while retaining a compact traversal-oriented base.
2. **Exact localized maintenance with explicit dependency repair.** For BFS, VeloGraphX detects shortest-parent losses before destructive updates, invalidates only vertices whose shortest-path support disappears, repairs from valid boundary predecessors, propagates distance decreases, and conservatively recomputes when the affected region exceeds a bound.
3. **Pre-repair repair-versus-recompute selection.** VeloGraphX exposes both exact modes to a policy layer that combines structural preflight guards with recent measured arm costs, avoiding both unnecessary global work and the repair-then-recompute double-work path when possible.
4. **A reproducible characterization of the crossover.** The evaluation uses pinned data and revisions, repeated raw measurements, exactness gates, explicit timing semantics, current-policy oracle metrics, external baselines, and negative-result retention.

The strongest claim is therefore not that VeloGraphX is universally the fastest graph system. It is that the preferred exact execution mode changes materially across dynamic regimes, and system architecture should make this choice explicit, observable, and adaptable.

## 2. Problem and execution model

Let graph state `G_t` result from applying update batch `U_t` to `G_{t-1}`. For an exact graph analytic `F`, the system must produce `R_t = F(G_t)` after every batch. We consider two physical strategies:

- **Full recomputation**, which applies `U_t` and computes `F(G_t)` from scratch.
- **Localized repair**, which uses `R_{t-1}`, `U_t`, graph structure, and algorithm-specific dependency information to transform the prior exact result into `R_t`.

Both target the same semantics. Their costs differ with graph size, update size, reachability, destructive-change location, affected-region size, mutable-storage state, and recent observed execution cost.

For batch `t`, let `C_inc(t)` be the measured answer-ready cost of exact incremental maintenance and `C_full(t)` the measured answer-ready cost of exact recomputation under the same updated graph and timing envelope. The offline per-batch oracle cost is

`C_oracle(t) = min(C_inc(t), C_full(t))`.

A selector chooses arm `a_t` before execution and incurs `C_a(t)` plus its measured decision cost. We report oracle-relative regret `(C_a(t)-C_oracle(t))/C_oracle(t)`, wrong-arm frequency, selector overhead, explicit full choices, and internal fallbacks. Regret captures the magnitude of a mistake; wrong-arm frequency alone does not, because a wrong choice near the crossover can be inexpensive while the same mistake in a strongly separated regime can be costly.

### 2.1 Correctness contract

Exactness is non-negotiable. Every maintained result used in performance comparison is checked against an independently computed exact reference outside the timed region. A policy may choose recomputation and an incremental implementation may conservatively fall back without weakening semantics. The selector changes only **how** an exact result is obtained.

The contract has three implications. First, the policy never trades accuracy for latency. Second, a policy error is a performance error rather than a semantic error. Third, exact recomputation remains both an execution arm and an experimental oracle/reference, making it possible to reason about adaptation without changing the logical query.

### 2.2 Timing contract

We distinguish answer-ready latency from process-wall costs that include loading or representation construction. Comparisons use only compatible timing envelopes. Selector decision time is recorded separately and included in adaptive batch timing where specified by the publication harness. Exactness verification is deliberately outside the timed region but is mandatory for accepting a result.

Absolute timings from separate GitHub-hosted campaigns are not merged into a synthetic ranking. Same-run and paired hosted measurements support scoped relative claims on the evaluated runner; they do not establish universal peak throughput or many-core scalability.

### 2.3 Execution lifecycle

A dynamic BFS batch passes through four conceptual stages:

1. **Preflight.** The selector observes only information available before running the chosen arm: update density, graph/reachability scale, shallow destructive dependencies where the harness computes them, previous affected work, and recent cost history.
2. **Arm selection.** The policy chooses localized repair or full recomputation. The choice is logged with a reason and decision cost.
3. **Exact execution.** The update batch is applied through the selected path. If localized deletion repair determines that its dependency region exceeds the safety/performance bound, it may still fall back internally.
4. **Telemetry update and verification.** The observed arm cost and affected-work state update the selector's history. An independent exact BFS is computed outside timing for validation in the publication harness.

Separating these stages makes adaptation observable: the artifact records whether full execution was selected before repair, reached through fallback, or would have been preferable only in hindsight.

## 3. System design

### 3.1 Mutable graph substrate

The storage design separates a compact base representation from mutable state. The base is segmented CSR with sorted, deduplicated adjacency. Segments contain 65,536 logical vertices, allowing untouched regions to retain contiguous CSR traversal while updates remain local. For undirected input the logical representation materializes both directions; directed graphs maintain both forward and reverse adjacency so predecessor-dependent repair does not require global source scans.

Updates are represented in packed mutable delta arenas. Each logical row owns metadata for a sorted slice of delta entries, and an entry represents desired presence relative to the compact row. If later updates restore an edge to its compact state, the overlay entry disappears; the live delta ratio therefore tracks current divergence rather than cumulative update history. This is important for policy observability because a long update history does not automatically imply a large active overlay.

Sparse compaction materializes only dirty logical rows into row-level compact patches instead of rebuilding an entire 65K-vertex CSR segment. A patched row becomes the compact reference for later deltas, while untouched rows continue reading from the original CSR. Forward and reverse rows are maintained symmetrically. Normal updates therefore avoid an O(E) canonical rebuild.

Over long executions, however, many row patches can accumulate and increase both owned memory and lookup cost. VeloGraphX exposes explicit canonical CSR consolidation that materializes the current logical graph into a fresh segmented CSR plus transpose. The source graph remains unchanged until the caller validates and cuts over to the new snapshot. Consolidation is thus a maintenance boundary, not an implicit side effect of `apply()`.

This layout balances two costs. Immediate whole-graph reconstruction simplifies subsequent traversal but turns every update epoch into O(E) maintenance. Retaining too much mutable state reduces update cost but eventually increases traversal and memory overhead. VeloGraphX therefore treats consolidation as a bounded maintenance decision rather than an unconditional update step.

### 3.2 Batch normalization before BFS repair

A batch can contain repeated or contradictory updates to the same edge. Processing every historical operation independently would both waste work and complicate deletion dependency reasoning. The BFS maintenance path scans the batch from the end and retains only the final desired state for each canonical edge key. For undirected graphs the key is normalized so `(u,v)` and `(v,u)` identify the same logical edge.

Before applying destructive updates, VeloGraphX checks which final deletions actually exist in the current graph and which of those edges are shortest-parent edges under the maintained distances. Only such deletions can immediately remove shortest-path support. The resulting candidate set is deduplicated before dependency propagation. This pre-batch view is essential: after the edge is removed, the system would otherwise lose direct evidence that the deleted edge had supported the old exact solution.

### 3.3 Exact deletion repair

For a vertex `v` with maintained distance `d(v)`, a predecessor `p` is a shortest parent when `d(p)+1=d(v)`. A deletion is potentially destructive only when it removes such support. VeloGraphX counts the number of shortest parents for candidate vertices and tracks how many of those supports are removed by the final batch.

A vertex becomes affected only when all of its shortest-parent support is lost. Once affected, it can in turn remove shortest-parent support from descendants whose old distance is one larger. VeloGraphX therefore performs an invalidation propagation over the **pre-batch** dependency structure, excluding edges that the current batch itself deletes. This identifies the portion of the old BFS solution that cannot remain valid after the batch.

The invalidation is bounded. The default BFS deletion fallback fraction is 0.35: if affected vertices exceed 35% of the graph, localized repair stops and the algorithm requests exact recomputation. The bound is deliberately conservative. It guarantees no loss of exactness while preventing a nominally incremental path from traversing an almost graph-wide dependency region merely to prove that full BFS would have been cheaper.

If the affected set remains below the bound, the batch is applied and the invalidated vertices are first marked unreachable. Repair then proceeds from the boundary of unaffected state. For each affected vertex, VeloGraphX examines incoming neighbors that are still valid and takes the best available boundary distance. These seeds enter a min-heap so repaired distances propagate through the affected subgraph in nondecreasing distance order. Vertices with no valid boundary path remain unreachable.

This boundary-driven phase handles distance increases and disconnections caused by deletions. Any repaired vertex whose new distance is *smaller* than its saved pre-repair value seeds a normal decrease propagation, ensuring interactions between simultaneous deletions and insertions are handled consistently.

### 3.4 Exact insertion repair

Insertions are simpler because they cannot invalidate an existing shortest distance; they can only create a shorter path or make an unreachable vertex reachable. After deletion repair, each final addition `(u,v)` is relaxed. If `d(u)+1<d(v)`, `d(v)` decreases and `v` enters a FIFO propagation queue. The decrease is then propagated through outgoing neighbors exactly as in BFS relaxation. Undirected graphs relax both orientations.

The maintained result after both phases is therefore equivalent to a fresh BFS: destructive changes first remove unsupported old dependencies and rebuild from valid boundaries; constructive changes then propagate any newly shorter paths.

### 3.5 Why fallback and pre-repair selection are different

The incremental BFS fallback and the external selector solve different problems. Internal fallback is discovered **during dependency analysis** and exists to bound localized repair. If it fires, some incremental work has already been paid. The pre-repair selector instead tries to predict when full recomputation is preferable **before** entering that path.

This distinction is central to VeloGraphX. A system with only internal fallback is adaptive in a semantic sense but can still suffer double work. A pre-repair choice can avoid that cost, while retaining fallback for cases where structural consequences are not predictable cheaply enough from preflight signals.

### 3.6 Pre-repair adaptive execution

The publication harness evaluates `always_incremental`, `always_full`, a simple update-density threshold, and the current adaptive path. The current implementation records a trace containing update fraction, reachable fraction, shallow shortest-parent deletion fraction, previous affected fraction, predicted incremental and full costs, observation ages, chosen arm, and decision reason.

For smaller graphs, the selector first applies structural guards. Very large update fractions choose full recomputation directly. Extremely sparse reachable state can also favor full execution because incremental bookkeeping may have little useful state to preserve. For moderately sparse reachable state, a smaller update-density guard can trigger full execution. When both arms have recent observations, exponentially weighted moving averages predict incremental and full cost; incremental prediction is inflated by previous affected fraction, and full execution is selected only when the predicted incremental cost exceeds the full prediction by a margin.

For larger graphs, the policy avoids repeatedly paying for a second full calibration. The initial exact BFS already measures a full-computation baseline. The missing incremental arm is probed separately, and later incremental predictions are scaled by the change in update fraction and previous affected work. Prediction uncertainty is tracked from observed relative error. Full execution is selected when the lower confidence estimate for incremental cost exceeds the upper confidence estimate for full cost; overlapping uncertainty defaults toward incremental execution rather than declaring a confident full win.

The policy also contains explicit preflight guards for large update fractions and shallow destructive changes during cold start. Historical one-sided warm-up behavior is retained in development provenance, while the publication validation records that the obsolete redundant one-sided-full choice is absent in the current audited policy.

### 3.7 Cost history and freshness

Observed execution time updates an exponential moving average with weight 0.25. The policy separately ages incremental and full observations; stale measurements are not treated as equally trustworthy indefinitely. For large graphs it also maintains an exponential moving average of relative prediction error for each arm, bounded before being converted into confidence intervals.

This design is intentionally simple. VeloGraphX does not claim a learned model or globally optimal scheduler. The purpose is to show that a low-cost, observable pre-repair policy can exploit the repair/recompute crossover while preserving a clear failure mode when its prediction is wrong.

### 3.8 Telemetry and explainability

Every adaptive batch records why an arm was selected and how much the selection itself cost. This supports three analyses that are difficult when adaptation is hidden inside an algorithm: (i) distinguishing deliberate full choices from internal fallback, (ii) measuring decision overhead independently of execution, and (iii) identifying wrong-arm regions such as the large `web-Google` tail.

The policy therefore produces not only an answer but also a physical-plan trace. That trace is part of the reproducibility contract and is retained in publication artifacts.

### 3.9 System boundary beyond BFS

The same mutable substrate is shared by additional exact or exact-with-conservative-fallback analytics: unweighted and weighted SSSP, connected components, triangles, k-core, and PageRank-related maintenance. The paper does not claim that the BFS selector transfers unchanged to every algorithm. Rather, BFS demonstrates the adaptive physical-plan architecture in depth, while triangle and storage experiments show that localized-versus-global crossover behavior also appears outside the primary BFS path.

## 4. Design rationale and alternatives

### 4.1 Why not rebuild CSR after every batch?

Doing so gives the simplest traversal representation but couples update cost to graph size. For small update fractions this can erase the benefit of localized analytics before the algorithm even begins. Packed deltas and row patches keep update work proportional to changed state for longer, while explicit consolidation provides a controlled route back to a canonical representation.

### 4.2 Why not always repair until the algorithm falls back?

Because discovering that repair is expensive can itself be expensive. The 35% deletion fallback bound protects the incremental algorithm, but it does not refund dependency-discovery work already performed. Pre-repair selection attacks that separate source of waste.

### 4.3 Why not choose only from update fraction?

Update fraction is useful but incomplete. A small destructive batch near shallow shortest-path levels can affect more state than a larger batch in an irrelevant region. Similarly, the same fractional batch can imply very different absolute work at different graph scales. VeloGraphX therefore combines update density with reachability, shallow dependency information, prior affected work, and recent arm costs.

### 4.4 Why not hide the selector inside BFS?

Keeping the selector outside the maintained algorithm gives full recomputation equal status as a physical plan and makes policy behavior independently measurable. It also prevents the paper from conflating algorithm correctness, fallback correctness, and plan-selection quality.

### 4.5 Failure modes

The selector can fail in two main ways. A **false incremental** decision chooses repair when full recomputation is cheaper, potentially exposing a large regret tail. A **false full** decision sacrifices useful maintained state and pays a global traversal unnecessarily. Both remain exact. The evaluation therefore reports not only average regret but also p95/max regret, wrong-arm rate, full-choice fraction, and internal fallback.

The current results show why this decomposition matters: average regret is low, yet one large `web-Google` regime remains visibly difficult. That tail is a plan-quality limitation, not a correctness limitation.

## 5. Experimental methodology

### 5.1 Research questions

**RQ1 — Correctness and crossover.** Do exact localized repair and full recomputation exchange the performance lead as update impact changes, and can the current pre-repair policy track the preferable arm without sacrificing exactness?

**RQ2 — Selector behavior.** When the policy makes a wrong choice, is the error frequent, expensive, or concentrated in particular regimes? Does it avoid repair-then-full double work?

**RQ3 — External dynamic BFS comparison.** How does VeloGraphX compare with established dynamic graph systems under paired/same-run semantics, and do conclusions change by graph family?

**RQ4 — Static execution context.** Is the underlying engine competitive with optimized static graph libraries, and where does it lose?

**RQ5 — Algorithmic breadth.** Do exact dynamic mechanisms provide meaningful benefits beyond BFS under a semantically fair published-reference comparison?

**RQ6 — Storage maintenance.** Does delaying whole-graph canonicalization reduce maintenance cost at large graph scale, and what memory trade-off does it impose?

### 5.2 Reproducibility discipline

Datasets are checksum-pinned and preprocessing is recorded. External systems are pinned to immutable revisions where possible. Raw repetitions are retained before aggregation. Dynamic workloads verify exactness after every measured execution. The benchmark record retains competitor wins and negative results. Machine-readable artifacts record environment and timing semantics.

Hosted CI is used as reproducible comparative evidence when alternatives execute under the same documented envelope. We do not use hosted evidence to claim stable 8/16/32-core scaling, true multi-socket NUMA behavior, hardware-counter superiority, NVMe performance, or universal peak throughput.

### 5.3 Statistical reporting

We report medians for repeated latency measurements and dispersion where raw samples permit it. Policy experiments additionally report oracle-relative regret, tail regret, wrong-arm rate, full-recomputation frequency, internal fallback, and selector overhead. Averages do not replace per-dataset or per-regime results when winner reversals occur.

The adaptive-policy experiments execute both exact arms in the publication harness so the offline oracle is measured rather than modeled. The production decision sees only pre-execution state and prior observations; oracle information is used only after the fact for evaluation.

### 5.4 Scope of hosted evidence

Shared hosted runners are valuable because they are reproducible and permit paired or same-run comparisons, but they are not stable microarchitectural testbeds. We therefore interpret them at the granularity the experiments support: relative behavior under the same run, crossover direction, exactness, and policy regret. Claims that fundamentally depend on socket topology, persistent NUMA placement, device bandwidth, or wide many-core scaling remain outside the headline scope.

## 6. Evaluation

### 6.1 Current selector: exact, low average regret, visible tail

The primary current-policy campaign is GitHub Actions run `34929398888` with retained artifact `10381490811`. It evaluates checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google`, one fixed root per graph, three batch regimes per graph, five repetitions per regime, and one thread. The historical graph/root program is reused for cross-dataset current-policy validation; we do not describe it as a newly unseen holdout.

Across **nine regimes and 1,610 adaptive batch samples**, all policy outputs match exact BFS. The current selector records **3.939% mean oracle regret across regimes**, **2.309% sample-weighted mean regret**, a **1.739% sample-weighted wrong-arm rate**, **zero internal full fallbacks**, and about **0.286 µs sample-weighted decision cost**. The obsolete redundant one-sided-full decision is never taken.

The result is not uniformly near-oracle. On `soc-Epinions1`, all three regimes have zero wrong-arm selections and mean regret between 0.86% and 1.10%. On `ca-GrQc`, the selector moves from all-incremental at batch 96 to all-full at batch 1,536; the middle batch has an 18.9% wrong-arm rate but 2.83% mean regret. The largest `web-Google` regime, batch 24,576, is the principal tail weakness: mean regret is **17.477%**, p95 regret **54.424%**, and wrong-arm rate **33.3%** over 15 batch samples. This regime is retained in the primary result rather than tuned away.

A separate focused `web-Google` regression run (`34928935983`, artifact `10381480310`) validates the current policy across four other batch sizes. It is 100% exact, records 2.466% mean regret across regimes, 15.671% worst-regime p95 regret, 27.717% maximum single-batch regret, zero internal fallbacks, and zero redundant one-sided-full decisions. We use this as regression evidence rather than as a cross-dataset claim.

Together these experiments support the central thesis without claiming oracle optimality: exact repair and recomputation have a real crossover, and a pre-repair policy can capture much of that benefit while still exposing a measurable tail where the decision remains difficult.

### 6.2 Interpreting policy errors

The aggregate wrong-arm rate is much smaller than the mean-regret number might suggest because most samples occur in regimes where the selector's preference is stable. `soc-Epinions1` is the clearest example: all three regimes make no wrong-arm selections, yet small nonzero oracle regret remains because measured timings vary even when the same physical arm is chosen. Conversely, the middle `ca-GrQc` regime demonstrates that wrong-arm frequency need not imply catastrophic cost: 18.9% wrong-arm selections coexist with only 2.83% mean regret because the two exact arms are relatively close near the crossover.

The largest `web-Google` regime is qualitatively different. It combines a 33.3% wrong-arm rate with a 17.48% mean regret and 54.42% p95 regret, showing that the arm separation is larger when the policy misses. The result suggests that recent cost history and update fraction do not fully capture the destructive structural impact of this regime. Because the campaign is a historical fixed graph/root program rather than a fresh held-out workload, we deliberately do not retune thresholds against this tail after observing it.

Zero internal fallbacks across the current-policy validation is also significant for the architecture. The adaptive path is not achieving low average regret by repeatedly entering incremental repair and then escaping to full BFS. Full executions recorded by the policy are explicit pre-repair choices. This separates selector quality from the incremental algorithm's 35% safety fallback and shows that the measured current-policy path avoids that particular form of double work on these experiments.

### 6.3 Dynamic BFS versus NetworKit

The accepted NetworKit campaign uses NetworKit 11.2.1, one thread, two graph families, three fixed roots per dataset, and five paired repetitions per root. All 30 paired executions are exact. On `web-Google`, the mean VeloGraphX/NetworKit paired latency ratio is about 0.73, corresponding to approximately **1.38× lower latency for VeloGraphX** on that evaluated workload. On `ca-GrQc`, the ratio is about 1.35, so **NetworKit is approximately 1.35× faster**.

The reversal is important: external-system conclusions are workload-specific. It also aligns with the paper's central premise that graph/update structure affects which execution machinery pays off. We do not generalize these two datasets to universal superiority.

### 6.4 Static BFS and SSSP versus GAP and LAGraph

A same-run hosted campaign compares VeloGraphX with GAP Benchmark Suite and LAGraph/SuiteSparse:GraphBLAS at 1, 2, and 4 threads, with five repetitions per configuration and independent correctness checks. VeloGraphX is fastest in the evaluated BFS cases, measuring **1.60×–2.04× faster than GAP** and **9.4×–11.8× faster than LAGraph**. Weighted SSSP gives the opposite lesson: **GAP is fastest**; VeloGraphX is 2.6×–3.0× faster than LAGraph but 7.0×–8.5× slower than GAP.

These results establish two useful boundaries. First, the full-recompute arm used by the adaptive architecture is not merely an intentionally slow reference path; the underlying BFS engine is competitive on the evaluated hosted cases. Second, the weighted-SSSP loss prevents the manuscript from implying that every kernel receives the same degree of optimization.

### 6.5 Exact dynamic triangles versus a published exact reference

We compare VeloGraphX with the exact `GoldenCounter` implementation distributed with published SIGMOD 2021 source code, pinned to an immutable revision. Because `GoldenCounter::insert_edge` does not itself make the exact global triangle answer available, the comparable baseline is insertion plus the subsequent exact `triangle_count()` query.

On normalized `facebook-combined` (4,039 vertices and 88,234 undirected edges), all 15 paired comparisons at 1%, 5%, and 10% insertion batches produce identical exact counts. Median VeloGraphX answer-ready latency is 1.066 ms, 6.782 ms, and 15.495 ms, versus 43.657 ms, 47.095 ms, and 53.931 ms for the published exact reference: **40.95×, 6.94×, and 3.48× lower answer-ready latency**, respectively.

The declining advantage as the batch grows is itself consistent with the localized-versus-global story: more changed state reduces the fraction of work that exact incremental maintenance can avoid. This is evidence about the pinned exact reference component, not a claim that VeloGraphX outperforms the paper's approximate sliding-window algorithm, whose semantics differ.

### 6.6 Large-graph storage maintenance

The accepted canonicalization A/B uses SNAP `com-Orkut`, with 3,072,441 vertices and 234,370,166 directed arcs in the dynamic representation, over 60 mutation/maintenance epochs. It compares a conservative 1.25× owned-storage envelope with a bounded 1.50× large-graph policy while preserving exact edge counts and consolidation digests.

The wider bounded envelope reduces consolidations from 15 to 6 and total consolidation time from 386.573 s to 156.128 s, a **59.6% reduction**. Maintenance-amortized throughput increases from 19,135 to 43,062 operations/s, or **2.25×**. The trade-off is higher peak RSS: 8,016,740 KiB versus 7,517,892 KiB, approximately **6.6% higher**.

This experiment shows that whole-graph canonicalization can dominate maintenance at 100M+-arc scale and that a bounded delay can materially improve amortized cost. It does not establish that the 1.50× threshold is universally optimal. Instead, the result motivates treating canonical reconstruction as another explicit global operation whose frequency must be controlled rather than assumed free.

### 6.7 Supporting breadth and maturity

A fresh three-dataset triangle crossover campaign retains five repetitions at 13 update fractions per dataset and exactness against full recomputation. `facebook-combined` remains incremental-favorable through the largest tested ratio, while `p2p-Gnutella08` and `ca-HepTh` cross into full-recompute-favorable regimes at sufficiently large updates. This supports the broader graph-dependent crossover story but is secondary to the BFS policy experiment.

Hosted engineering campaigns also show 4-thread speedups of 2.74× for BFS, 2.50× for connected components, and 2.24× for triangle counting, and compression ratios of approximately 3.25×–3.78× with a documented traversal trade-off. These are implementation-maturity results, not many-core claims.

## 7. Discussion

### 7.1 The crossover is a systems property, not one threshold

The evaluation does not reveal a single global update fraction at which recomputation should always replace repair. `soc-Epinions1`, `ca-GrQc`, and `web-Google` behave differently, and the triangle campaign exhibits dataset-specific crossover points as well. A fixed threshold can therefore be useful as a baseline but cannot encode all structural consequences of an update batch.

The architecture is more important than any one current policy: both exact arms are available behind the same logical operation, the choice occurs before repair when possible, and telemetry is available to improve the policy without rewriting the maintained algorithm.

### 7.2 Negative results are part of the result

The benchmark record contains several useful reversals: full recomputation wins when repair becomes sufficiently broad; NetworKit wins the evaluated `ca-GrQc` dynamic-BFS workload; GAP substantially wins weighted SSSP; RisGraph remains faster on the documented hosted dynamic-BFS run; and the current selector has a visible large-`web-Google` tail. Removing these cases would weaken rather than strengthen the systems claim, because the paper is about workload-dependent execution choices.

### 7.3 Repair discovery can itself be wasted work

A selector that invokes incremental repair and only later discovers that the affected region is too large may pay twice: once for repair discovery or partial processing and again for full recomputation. This motivates pre-execution signals and selector-owned recomputation. The current publication-policy artifacts show zero internal fallbacks in the evaluated current-policy runs, so the policy is avoiding that specific double-work path there; the remaining error is mostly choosing the slower exact arm directly.

### 7.4 Storage and algorithm adaptation are related but distinct

The storage policy and analytical selector embody the same broad principle—avoid global work while localized state remains economical—but operate at different layers and timescales. We do not collapse them into one learned policy. Storage consolidation is a bounded maintenance decision; repair-versus-recompute selection is an answer-ready execution decision.

### 7.5 What the `web-Google` tail teaches

The large-regime tail is useful because it identifies what the current selector does **not** yet model well. Recent execution cost and update density summarize workload history, but they are imperfect proxies for future dependency expansion. Richer cheap features could estimate destructive locality, root-distance distribution, or sampled dependency exposure before repair. Any such extension should be evaluated under the same frozen-harness discipline rather than tuned until the visible tail disappears.

### 7.6 Generalization beyond BFS

The architecture does not require every analytic to use the same features or thresholds. An exact triangle counter, k-core maintainer, or PageRank repair path exposes different dependency state. What transfers is the interface: a localized exact arm, a full exact arm, pre-execution observables, explicit fallback, and post-execution telemetry. BFS is therefore a detailed case study of the physical-plan abstraction rather than a claim that one selector is universal.

## 8. Limitations

The primary comparative evidence is hosted and therefore intentionally scoped. The paper does not claim stable many-core scaling, multi-socket NUMA behavior, hardware-counter advantages, NVMe/out-of-core superiority, or universal peak throughput. NetworKit evidence covers one thread and two graph families. The accepted RisGraph and NetworKit campaigns were executed separately, so their absolute latencies are not combined into a three-system ranking unless the unified same-machine campaign is separately audited.

The current policy's three-graph validation uses a historical fixed graph/root program rather than a newly preregistered unseen holdout. Its average regret is low, but the largest `web-Google` regime has a material tail; the paper therefore does not claim uniform near-oracle behavior. The system supports multiple maintained analytics, but adaptive selection is studied most deeply for BFS. Some destructive weighted-SSSP updates conservatively recompute. Python and other 0.x APIs may evolve.

Historical selector-development experiments changed multiple mechanisms across iterations. They motivate the final design but are not a clean cumulative component ablation. If the manuscript makes causal claims about individual selector mechanisms, it should add explicit feature switches under one frozen harness; otherwise mechanism discussion remains descriptive.

The storage result shows a clear canonicalization trade-off on one very large graph, but it does not determine a universally optimal envelope. A broader controlled-hardware study could vary mutation locality, memory pressure, graph family, and consolidation implementation. Those questions are useful follow-up work rather than prerequisites for the paper's repair-versus-recompute thesis.

## 9. Related work

### 9.1 Dynamic and incremental graph processing

GraphIn explicitly introduced a property-based dual-path execution model that can switch between incremental and static processing. VeloGraphX therefore does not claim that the existence of two graph-processing paths is new. The distinction here is the exact repair/recompute framing over one mutable analytical state, the pre-repair placement of the decision, and explicit accounting for wrong-arm and repair-to-full double work.

Bok et al. predict incremental-processing cost from prior history and select between incremental and static execution. This establishes that history-based cost selection is also prior art. VeloGraphX combines recent cost observations with structural preflight guards and keeps selector telemetry alongside exactness/fallback evidence; the contribution is the integrated exact systems design and evaluation rather than the general idea of using past cost.

GraphBolt and DZiG develop dependency-driven and sparsity-aware incremental graph processing. Their work motivates exploiting only affected state rather than rerunning a whole computation. VeloGraphX's BFS repair likewise uses dependency structure, but the paper's focus is on when to expose that repair as one physical arm versus choosing recomputation before repair begins.

RisGraph targets low-latency evolving-graph processing and provides an important external dynamic-system reference. Layph addresses broad propagation in dynamic graph computation through layered processing. These systems reinforce that dynamic graph performance depends on how change propagates; VeloGraphX studies the complementary plan-selection question under exact repair/recompute alternatives.

### 9.2 Mutable graph storage

Dynamic storage itself is not claimed as novel. GraphOne uses a hybrid representation supporting graph updates and analytical views, while Teseo develops a sophisticated mutable graph representation with transactional support. VeloGraphX's segmented CSR, packed deltas, and sparse row patches should be read as the substrate required to make repeated exact repair and explicit consolidation practical, not as a claim to have invented mutable graph storage.

The storage contribution in this paper is consequently scoped to its interaction with execution: updates need not force canonical reconstruction, reverse adjacency supports dependency repair, and canonicalization is exposed as an explicit bounded maintenance action whose cost can be measured separately.

### 9.3 Static graph analytics and sparse linear algebra

GAP Benchmark Suite provides optimized graph kernels and a widely used evaluation reference; LAGraph/SuiteSparse:GraphBLAS represents graph algorithms through sparse linear algebra. The static comparison in this paper is contextual rather than a claim that VeloGraphX replaces either ecosystem. It verifies that the recomputation arm is credible on BFS while retaining the weighted-SSSP case where GAP is substantially faster.

NetworKit is a mature graph-analysis library with dynamic capabilities and serves as the paired external BFS baseline. The observed winner reversal across `web-Google` and `ca-GrQc` is consistent with the paper's decision not to make a universal fastest-system claim.

## 10. Conclusion

VeloGraphX treats exact localized repair and full recomputation as alternative physical strategies for the same evolving-graph analytics problem. A mutable graph substrate makes repeated updates practical, exact maintained algorithms expose localized work, and a pre-repair selector can avoid committing the system to one execution mode across all regimes.

The BFS design makes the boundary concrete: destructive updates remove shortest-parent support, dependency invalidation identifies affected state before the batch is applied, boundary repair restores exact distances, insertions propagate decreases, and a conservative fallback remains available when affected work becomes broad. Above that algorithm, a low-cost selector can choose full execution before entering repair and can record why it made that choice.

The evaluation deliberately retains regimes where recomputation, competitors, or the oracle beat the chosen policy, because those reversals are central to the result: dynamic graph execution has a crossover. The current policy is exact and low-regret on average, but its visible `web-Google` tail shows that plan selection remains a real systems problem rather than a solved threshold-tuning exercise.

The resulting systems lesson is simple: **incremental maintenance should be a selectable exact execution strategy, not an unconditional architectural assumption.**