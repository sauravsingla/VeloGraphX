# VeloGraphX: Adaptive Exact Analytics for Evolving Graphs

> Working manuscript. Quantitative claims in this file are restricted to `results-ledger.md`, `data/accepted-results.json`, `submission-closure-evidence.json`, and `../docs/paper-evidence-index.md`.

## Abstract

Graph analytics systems increasingly operate on graphs that change continuously, yet the execution strategy for maintaining an analytical result is often fixed in advance: either recompute after updates or maintain incrementally. Neither strategy dominates across regimes. Localized repair can avoid most graph work when changes have limited impact, while full recomputation becomes preferable when affected state grows or repair-discovery overhead accumulates.

We present **VeloGraphX**, a C++20 system that exposes localized repair and full recomputation as competing physical execution strategies over one mutable graph substrate. For dynamic BFS, both arms produce the same exact result; deletion repair detects loss of shortest-path support, reconstructs affected state from valid boundaries, and conservatively falls back when the affected region becomes too broad. A pre-repair selector sits outside the repair path so it can choose full recomputation before paying dependency-discovery cost. The system records plan decisions, costs, fallbacks, and exactness separately.

On the primary three-graph validation, covering nine regimes and 1,610 adaptive batch samples, all outputs are exact and the selector records **3.94% mean oracle regret across regimes**, **2.31% sample-weighted regret**, and **1.74% sample-weighted wrong-arm rate**; the largest `web-Google` regime retains a visible **17.48% mean / 54.42% p95** regret tail. A production-style replay with the normal **0.35 affected-region fallback** is exact across 93 aligned observations. All six fallback-only fallbacks occur in the declared 220K destructive-cascade stress case; the frozen pre-repair selector preempts all six and **17.323 ms** of conservatively measured repair-then-full double work. Across all 93 aligned observations, selector+fallback reduces cumulative answer-ready latency by **42.7%** relative to fallback-only; on the 72 real-graph observations, which trigger no 0.35 fallback, the reduction is **42.2%**. A frozen, no-retuning held-out campaign shows that selector quality is workload-dependent: unseen `Amazon0312` records **1.52%** equal-regime mean regret, while timestamp-ordered `CollegeMsg` records **34.52%**. A matched GraphBolt comparison likewise retains a winner reversal across update fractions. These results support the architectural conclusion rather than a universal selector claim: **incremental maintenance should be a selectable exact execution strategy, not an unconditional architectural assumption**.

## 1. Introduction

Large graphs are rarely static. Relationship, transaction, knowledge, infrastructure, and dependency graphs receive continuous insertions and deletions while applications continue to ask graph-analytic questions. After each update batch, an exact system faces a basic execution decision: should it repair only the state that may have changed, or should it recompute the result over the updated graph?

The usual framing makes one choice architectural. Static engines optimize full recomputation. Dynamic engines emphasize incremental maintenance. In practice, the boundary is workload-dependent. Small or structurally local updates can make localized repair much cheaper than a graph-wide traversal. Larger updates, destructive changes, or broad dependency cascades can reverse the ordering: the work needed to discover and repair affected state can approach or exceed the cost of a fresh computation. A system that hard-codes either strategy can therefore pay unnecessary work in regimes where the other strategy is preferable.

The difficulty is not merely update volume. Two batches with the same number of changed edges can have different consequences because they touch different positions in the dependency structure of the maintained result. A deletion close to the BFS root can invalidate a large dependency region, while a larger batch elsewhere can remain cheap to repair. Graph scale, reachability, destructive locality, recent arm costs, and uncertainty can therefore matter alongside update fraction. This makes repair-versus-recompute selection resemble a physical-plan choice rather than a fixed property of an algorithm.

VeloGraphX is built around the observation that **repair and recomputation are two exact physical execution strategies for the same logical result**. The system keeps both strategies available behind a common mutable graph representation. Incremental algorithms identify and repair affected state; conservative fallback preserves exactness; and a pre-repair selector uses observable state plus prior measured cost to choose how to execute a batch.

This placement matters. A dynamic algorithm can be semantically adaptive yet still waste work if it begins repair, discovers that the affected region is too large, and then performs a full computation. Correctness is preserved, but the system pays both dependency-discovery or partial-repair work and recomputation. VeloGraphX allows the policy to choose full execution **before** the incremental path begins. Internal fallback remains a safety/performance bound rather than the primary adaptation mechanism.

The same broad tension appears in storage. Rebuilding canonical CSR after every update defeats much of the benefit of localized analytics, while indefinitely accumulating mutable overlays increases traversal and maintenance cost. VeloGraphX separates a compact segmented-CSR base from packed deltas and sparse row patches and exposes explicit canonical consolidation under bounded policies. Storage and analytical adaptation are separate mechanisms, but both avoid global work while local state remains economical.

The paper focuses on exact dynamic BFS because it exposes insertion- and deletion-induced dependency changes and admits a direct exact recomputation oracle. The codebase also includes exact or conservative-fallback workflows for unweighted and weighted SSSP, connected components, triangles, and k-core. PageRank is described separately as **residual/tolerance-validated maintenance with conservative fallback**; we do not use PageRank to extend the paper's mathematical exactness claim.

The paper makes four contributions:

1. **A mutable graph substrate for repeated analytics.** Segmented CSR, packed deltas, sparse row patches, forward/reverse adjacency, and explicit consolidation avoid mandatory whole-graph reconstruction after every batch while preserving a compact traversal-oriented base.
2. **Exact localized BFS maintenance with explicit dependency repair.** VeloGraphX detects shortest-parent loss before destructive updates, invalidates only unsupported state, repairs from valid boundary predecessors, propagates decreases, and conservatively recomputes when the affected region exceeds a bound.
3. **Pre-repair repair-versus-recompute selection.** Both exact modes are exposed to a policy layer that can choose full execution before repair begins. Production-style evidence directly measures the repair-then-full work this placement can avoid.
4. **A reproducible evaluation that retains generalization failures and competitor wins.** The artifact includes a primary oracle-relative selector campaign, production fallback replay, frozen held-out temporal evaluation, clean feature ablation, matched GraphBolt evidence, external baselines, exactness gates, and archived claim boundaries.

The strongest claim is therefore not that VeloGraphX is universally fastest or that its current selector generalizes uniformly. It is that the preferred exact execution mode changes materially across dynamic regimes, and system architecture should make that choice explicit, observable, and adaptable.

## 2. Problem and execution model

Let graph state `G_t` result from applying update batch `U_t` to `G_{t-1}`. For an exact graph analytic `F`, the system must produce `R_t = F(G_t)` after every batch. We consider two physical strategies:

- **Full recomputation**, which applies `U_t` and computes `F(G_t)` from scratch.
- **Localized repair**, which uses `R_{t-1}`, `U_t`, graph structure, and algorithm-specific dependency information to transform the prior exact result into `R_t`.

Both target the same semantics. Their costs differ with graph size, update size, reachability, destructive-change location, affected-region size, mutable-storage state, and recent observed execution cost.

For batch `t`, let `C_inc(t)` be the measured answer-ready cost of exact incremental maintenance and `C_full(t)` the measured answer-ready cost of exact recomputation under the same updated graph and timing envelope. The offline per-batch oracle is

`C_oracle(t) = min(C_inc(t), C_full(t))`.

A selector chooses arm `a_t` before execution and incurs `C_a(t)` plus its measured decision cost. We report oracle-relative regret `(C_a(t)-C_oracle(t))/C_oracle(t)`, wrong-arm frequency, selector overhead, explicit full choices, internal fallbacks, and—in the production-fallback campaign—repair-discovery work paid before fallback.

### 2.1 Correctness contract

Exactness is non-negotiable for the BFS experiments. Every maintained result used in performance comparison is checked against an independently computed exact reference outside the timed region. A policy may choose recomputation and an incremental implementation may conservatively fall back without changing semantics. A policy mistake is therefore a performance error rather than an accuracy error.

This contract deliberately separates exact dynamic BFS from tolerance-based numerical workflows such as PageRank. The latter may share storage and maintenance machinery, but they are not evidence for the exactness contract or the exact repair/recompute oracle used in the paper.

**BFS exactness invariant.** Before a batch, maintained distances equal a fresh BFS. A final deletion can increase a distance only after all shortest-parent support for the old distance is removed; invalidation propagates over the pre-batch shortest-path dependency relation and therefore contains every vertex whose old label can become invalid. Boundary repair recomputes the minimum admissible labels for that affected region, and insertion relaxation monotonically propagates every newly shorter path. If the affected set exceeds the bound, a fresh BFS is used instead. Thus either execution path restores the same post-batch distances as exact recomputation; the selector changes cost, not semantics.

### 2.2 Timing contract

We distinguish answer-ready latency from process-wall costs that include loading or representation construction. Comparisons use only compatible timing envelopes. Selector decision time is recorded separately and included where specified by the publication harness. Correctness verification is outside timing but mandatory for accepting a result.

Absolute timings from unrelated GitHub-hosted campaigns are never merged into a synthetic ranking. Same-run and paired hosted measurements support scoped relative claims on the evaluated runner; they do not establish universal peak throughput, many-core scalability, or hardware-specific superiority.

### 2.3 Execution lifecycle

A dynamic BFS batch passes through four conceptual stages:

1. **Preflight.** Observe only information available before running the chosen arm: update density, graph/reachability scale, shallow destructive dependencies where available, prior state, and recent arm-cost history.
2. **Arm selection.** Choose localized repair or full recomputation and record the reason and decision cost.
3. **Exact execution.** Apply the selected path. If localized deletion repair discovers an affected region beyond its bound, it may still fall back internally.
4. **Telemetry update and verification.** Update cost history and independently verify the resulting BFS outside the timed region.

The artifact records whether full execution was selected before repair, reached through fallback, or would have been preferable only in hindsight.

## 3. System design

### 3.1 Mutable graph substrate

The storage design separates a compact base representation from mutable state. The base is segmented CSR with sorted, deduplicated adjacency. Segments contain 65,536 logical vertices, allowing untouched regions to retain contiguous traversal while updates remain local. Directed graphs maintain both forward and reverse adjacency so predecessor-dependent repair does not require global source scans.

Updates are represented in packed mutable delta arenas. Each logical row owns metadata for a sorted slice of delta entries, and an entry represents desired presence relative to the compact row. If later updates restore an edge to its compact state, the overlay entry disappears; the live delta ratio therefore tracks current divergence rather than cumulative update history.

Sparse compaction materializes only dirty logical rows into row-level compact patches instead of rebuilding an entire segment. A patched row becomes the compact reference for later deltas, while untouched rows continue reading from the original CSR. Over longer executions, explicit canonical consolidation materializes the current logical graph into a fresh segmented CSR plus transpose. Consolidation is therefore a measurable maintenance boundary, not an implicit side effect of every update.

### 3.2 Batch normalization before BFS repair

A batch can contain repeated or contradictory updates to the same edge. The BFS maintenance path scans the batch from the end and retains only the final desired state for each canonical edge key. For undirected graphs the key is normalized so `(u,v)` and `(v,u)` identify the same logical edge.

Before applying destructive updates, VeloGraphX checks which final deletions actually exist and which removed edges are shortest-parent edges under the maintained distances. Only such deletions can immediately remove shortest-path support. This pre-batch view is essential: after an edge is removed, the system would otherwise lose direct evidence that it supported the old exact solution.

### 3.3 Exact deletion repair

For a vertex `v` with maintained distance `d(v)`, predecessor `p` is a shortest parent when `d(p)+1=d(v)`. A deletion is potentially destructive only when it removes such support. VeloGraphX counts shortest parents for candidate vertices and tracks how many are removed by the final batch.

A vertex becomes affected only when all shortest-parent support is lost. Once affected, it can remove shortest-parent support from descendants whose old distance is one larger. VeloGraphX therefore propagates invalidation over the **pre-batch** dependency structure, excluding edges deleted by the current batch. This identifies the part of the old BFS solution that cannot remain valid.

The invalidation is bounded. The default deletion fallback fraction is **0.35**: if affected vertices exceed 35% of the graph, localized repair stops and requests exact recomputation. The bound preserves semantics while preventing an incremental path from scanning an almost graph-wide dependency region merely to discover that a fresh BFS is preferable.

If the affected set remains below the bound, the invalidated vertices are marked unreachable and repaired from the boundary of unaffected state. Incoming neighbors that remain valid provide candidate distances; a min-heap propagates repaired distances through the affected subgraph in nondecreasing order. Vertices with no valid boundary path remain unreachable.

### 3.4 Exact insertion repair

Insertions cannot invalidate an existing shortest distance; they can only create a shorter path or make an unreachable vertex reachable. After deletion repair, each final addition is relaxed. Any decreased distance enters a FIFO queue and propagates through outgoing neighbors exactly as in BFS relaxation. The combined deletion and insertion phases therefore produce the same result as a fresh BFS.

### 3.5 Why fallback and pre-repair selection are different

Internal fallback and external selection solve different problems. Fallback is discovered **during** dependency analysis and bounds the localized algorithm. If it fires, some incremental work has already been paid. The pre-repair selector instead tries to choose full recomputation **before** entering that path.

This distinction is central to the paper because it is experimentally observable. A fallback-only system can remain exact yet waste work; the selector can avoid that work, while still retaining fallback for cases whose structural consequences are not predictable cheaply enough before execution.

### 3.6 Pre-repair adaptive execution

The publication harness evaluates `always_incremental`, `always_full`, a simple update-density threshold, a history-cost baseline, and the current adaptive policy `publication-preflight-v1`. The adaptive trace records update fraction, reachable fraction, shallow shortest-parent deletion information, prior affected state, predicted arm costs, observation ages, chosen arm, decision reason, and decision time.

For smaller graphs, the selector applies structural guards and recent measured arm costs. For larger graphs, the initial exact BFS provides a full-computation baseline and the missing incremental arm is probed separately. Prediction uncertainty is tracked from observed error. A full choice is made when the incremental estimate is sufficiently worse than the full estimate; overlapping uncertainty remains conservative rather than declaring a confident full win.

The exact numeric thresholds are frozen in the artifact. Operationally, the selector (1) computes pre-execution structural signals, (2) uses the initial BFS as a measured full-cost baseline, (3) probes a missing incremental arm rather than inventing its cost, (4) maintains exponentially smoothed observed arm costs and prediction errors, and (5) chooses full only when a structural guard fires or the uncertainty-adjusted incremental estimate is confidently worse; otherwise it chooses repair. Only the executed arm updates future cost history, so offline oracle costs never leak into later production decisions. The paper does not claim the hand-designed selector is globally optimal. Its role is to demonstrate that plan selection is measurable, inexpensive relative to execution, and separable from algorithm correctness.

### 3.7 Telemetry and explainability

Every adaptive batch records why an arm was selected and how much the selection cost. This supports three analyses: distinguishing deliberate full choices from internal fallback, measuring decision overhead independently of execution, and locating wrong-arm regimes. The policy therefore produces both an exact answer and a physical-plan trace.

### 3.8 System boundary beyond BFS

The mutable substrate is also used by other graph analytics. Exactness claims in this paper are strongest for the discrete algorithms whose outputs are independently verified. PageRank is intentionally worded differently: its maintenance path is residual/tolerance validated with conservative fallback. The BFS selector is not claimed to transfer unchanged to every algorithm.

## 4. Design rationale and alternatives

### 4.1 Why not rebuild CSR after every batch?

Whole-graph reconstruction gives a simple traversal representation but couples every update epoch to graph size. Packed deltas and row patches keep update work local for longer, while explicit consolidation provides a controlled route back to a canonical representation.

### 4.2 Why not always repair until fallback?

Because discovering that repair is expensive can itself be expensive. The 35% deletion fallback protects the incremental algorithm, but it does not refund dependency-discovery work already performed. The production-style fallback experiment directly measures this distinction.

### 4.3 Why not choose only from update fraction?

Update fraction is useful but incomplete. The same number of changed edges can have very different dependency consequences. The clean ablation therefore separates structural preflight, uncertainty, and previous-affected-work rather than treating the final policy as an indivisible heuristic.

### 4.4 Why keep the selector outside BFS?

Keeping the selector outside the maintained algorithm gives full recomputation equal status as a physical plan and makes policy behavior independently measurable. It also prevents the paper from conflating algorithm correctness, fallback behavior, and plan-selection quality.

### 4.5 Failure modes

A **false incremental** decision chooses repair when full recomputation is cheaper and may expose a large regret tail. A **false full** decision discards useful maintained state and pays a global traversal unnecessarily. Both remain exact. The evaluation therefore reports averages, tails, wrong-arm behavior, false-full choices, and fallbacks rather than only the best aggregate number.

## 5. Experimental methodology

### 5.1 Research questions

**RQ1 — Crossover and exactness.** Do exact localized repair and full recomputation exchange the performance lead as update impact changes, and can a pre-repair policy exploit that crossover without sacrificing exactness?

**RQ2 — Fallback double work.** Under the normal 0.35 affected-region bound, does pre-repair selection actually avoid repair-discovery work that a fallback-only path would pay before recomputation?

**RQ3 — Frozen generalization.** Without retuning after seeing results, how does the current selector behave on unseen roots/graphs and on a genuine timestamp-ordered interaction stream?

**RQ4 — Policy mechanisms.** Which selector mechanisms are supported by a clean one-factor-at-a-time ablation under one frozen harness?

**RQ5 — External systems.** How does VeloGraphX compare with matched dynamic baselines, and do winners change with graph or update regime?

**RQ6 — Breadth and storage.** Do the broader implementation and storage mechanisms show useful behavior without requiring a universal fastest-system claim?

### 5.2 Reproducibility discipline

Datasets are checksum-pinned and preprocessing is recorded. External systems are pinned to immutable revisions where possible. Raw repetitions are retained before aggregation. Dynamic workloads verify exactness after every measured execution. Negative results, competitor wins, and tail failures are retained rather than removed after inspection.

The primary selector program is historical and fixed, so it is **not** described as a new holdout. The held-out campaign freezes the selector before acquiring/running the declared unseen workloads and explicitly prohibits post-result retuning. `CollegeMsg` preserves observed timestamp arrival order; repeated interactions are idempotent under simple-graph semantics, and sliding-window removals are induced expiries rather than observed deletion events.

### 5.3 Statistical reporting

Repeated latency experiments report medians and dispersion where the retained raw samples support them. Policy experiments report oracle-relative regret, wrong-arm behavior, explicit full choices, fallback, and decision overhead. Equal-regime summaries prevent long regimes from silently dominating the result; sample-weighted values are also reported where appropriate.

The publication selector harness executes both exact arms to measure the offline oracle. Production decisions see only pre-execution state and prior observations; oracle information is used only after the fact for evaluation.

### 5.4 Hosted evidence scope

Hosted CI is used as reproducible comparative evidence when alternatives execute under the same documented timing envelope. We do not use hosted evidence to claim stable many-core scaling, multi-socket NUMA behavior, hardware-counter superiority, NVMe performance, or universal peak throughput.

## 6. Evaluation

### 6.1 Primary selector: exact, low average regret, visible tail

The primary current-policy campaign is run `34929398888`, retained artifact `10381490811`. It evaluates checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google`, one fixed root per graph, three batch regimes per graph, five repetitions per regime, and one thread. This historical graph/root program is reused for current-policy validation and is not claimed as an unseen holdout.

Across **nine regimes and 1,610 adaptive batch samples**, all outputs match exact BFS. The selector records **3.939% mean oracle regret across regimes**, **2.309% sample-weighted mean regret**, **1.739% sample-weighted wrong-arm rate**, zero internal fallbacks, and about **0.286 µs** sample-weighted decision cost.

The result is not uniformly near-oracle. The largest `web-Google` regime, batch 24,576, records **17.477% mean regret**, **54.424% p95 regret**, and **33.3% wrong-arm rate** over 15 batch samples. That tail remains in the headline evidence rather than being tuned away. A separate focused `web-Google` regression run (`34928935983`, artifact `10381480310`) remains exact and records 2.466% mean regret across its tested regimes; we use it as supporting regression evidence, not as a replacement for the cross-dataset result.

### 6.2 Production 0.35 fallback: directly measuring avoided double work

The earlier primary policy harness deliberately disabled normal internal fallback to isolate the repair and recomputation arms. That design is appropriate for oracle measurement but cannot by itself establish the practical benefit of avoiding “repair, discover broad impact, then recompute.” We therefore run a separate production-style replay with the normal **0.35 affected-region bound**.

Run `35237513376`, artifact `10503926632`, retains **93 aligned batch observations**, all exact. In the fallback-only path, internal fallback occurs **six** times, and **all six occur in the declared 220K destructive-cascade stress case**; none occurs in the 72 observations from `ca-GrQc`, `soc-Epinions1`, or `web-Google`. Under the same retained cases, the frozen pre-repair selector chooses full execution before repair in all six fallback opportunities, leaving **zero** selector-path internal fallbacks. The campaign conservatively measures **17.323 ms** of repair-then-full double work in the fallback-only path and **17.323 ms avoided** by pre-repair selection.

The retained summarizer also reports 33 `false_full` labels. That historical field means only that the selector chose full on a batch where the fallback-only path would not itself have fallen back; it is **not** a wrong-arm test and does not establish that full was slower than repair. Indeed, the retained positive `false_full_penalty_us` is zero for all 33 such labels. Across all 93 aligned observations, selector+fallback reduces cumulative answer-ready latency by **42.7%** relative to fallback-only (fallback-only / selector+fallback = **1.745×**). Restricting to the 72 real-graph observations, the reduction is **42.2%** (**1.729×**). These are campaign-scoped, sample-weighted totals; the cascade experiment demonstrates the fallback mechanism and its measurable cost, not the natural frequency of fallback events.

### 6.3 Frozen held-out evaluation: generalization is workload-dependent

To separate design validation from post-hoc tuning, run `35237513595`, artifact `10504131673`, freezes `publication-preflight-v1` before the held-out workloads are executed. The campaign remains exact and performs **no post-result retuning**.

Across all **15 held-out regimes**, equal-regime mean regret is **21.32%**, with a **185.83% worst-regime mean** and **311.88% worst-regime p95**. This aggregate hides an important split. On unseen `Amazon0312` (six regimes), equal-regime mean regret is only **1.52%**, worst-regime mean **2.29%**, and worst-regime p95 **9.44%**. On timestamp-ordered `CollegeMsg` (nine regimes), equal-regime mean regret is **34.52%**, with the **185.83% / 311.88%** worst-regime mean/p95 tail.

The correct interpretation is not that the held-out campaign invalidates the architecture. It invalidates a stronger claim that the current hand-designed selector is uniformly low-regret out of sample. The physical-plan abstraction generalizes; this particular policy remains workload-dependent. We retain the `CollegeMsg` failure and do not retune against it.

### 6.4 Clean feature ablation

Run `35237513377`, artifact `10504591544`, evaluates one-mechanism-at-a-time selector variants under one frozen graph/root/batch/repetition/timing contract. Every variant remains exact. The full adaptive policy records **4.82%** equal-regime mean regret in this ablation campaign. Removing structural preflight increases that value to **8.16%** and worsens the tail. Removing uncertainty handling is much more damaging: mean regret rises to **142.97%**, with **1160.11% worst-regime mean** and **3558.37% worst-regime p95**.

By contrast, removing the previous-affected-work factor yields **4.20%** mean regret, slightly better than the full policy on this campaign. We therefore do **not** claim that previous-affected-work is independently beneficial. The ablation supports structural preflight and uncertainty as useful mechanisms; it treats the affected-work feature as unresolved rather than forcing a positive story.

These values are not substituted for the primary 3.939% selector headline because the ablation is a different retained campaign with a different purpose.

### 6.5 Matched GraphBolt comparison

Run `35237513587`, artifact `10503851688`, compares VeloGraphX with a pinned legacy GraphBolt artifact runtime on the same retained mutation stream and hosted allocation. VeloGraphX is exact and GraphBolt's final BFS is independently verified on every retained run, with five paired repetitions per update fraction.

The GraphBolt/VeloGraphX answer-ready latency ratio is **14.219× at 0.1%**, **2.245× at 1%**, and **0.886× at 5%** operation fraction. Here operation fraction is the number of graph-operation rows divided by the initial-edge count, matching the retained workload generator (`0.01 = 1%`). Ratios above one indicate lower VeloGraphX latency; below one indicate lower GraphBolt latency. The winner therefore reverses as the update fraction grows. This is a workload-scoped comparison with a pinned artifact runtime, not a universal GraphBolt or DZiG ranking.

### 6.6 Dynamic BFS versus NetworKit

The accepted NetworKit campaign uses NetworKit 11.2.1, one thread, two graph families, three fixed roots per dataset, and five paired repetitions per root. All 30 paired executions are exact. On `web-Google`, VeloGraphX is approximately **1.38× faster** on the evaluated paired workload; on `ca-GrQc`, **NetworKit is approximately 1.35× faster**.

This reversal reinforces the paper's central point: external-system conclusions are workload-specific, and no universal fastest-system claim is warranted.

### 6.7 Static BFS and weighted SSSP context

A same-run hosted campaign compares VeloGraphX with GAP Benchmark Suite and LAGraph/SuiteSparse:GraphBLAS at 1, 2, and 4 threads, with five repetitions per configuration and correctness checks. VeloGraphX is fastest in the evaluated BFS cases, measuring **1.60×–2.04× faster than GAP** and **9.4×–11.8× faster than LAGraph**. Weighted SSSP gives the opposite result: **GAP is fastest**; VeloGraphX is 2.6×–3.0× faster than LAGraph but 7.0×–8.5× slower than GAP.

The purpose of this experiment is contextual. It shows that the full BFS arm is a credible execution path while retaining a kernel where another system clearly wins.

### 6.8 Exact dynamic triangles versus a published exact reference

Against the pinned exact `GoldenCounter` reference on normalized `facebook-combined`, all 15 paired comparisons at 1%, 5%, and 10% insertion batches produce identical exact counts. Median VeloGraphX answer-ready latency is 1.066 ms, 6.782 ms, and 15.495 ms, versus 43.657 ms, 47.095 ms, and 53.931 ms for the reference: **40.95×, 6.94×, and 3.48× lower answer-ready latency**, respectively.

This is evidence against the pinned exact reference component, not a claim against the paper's approximate sliding-window algorithm whose semantics differ.

### 6.9 Large-graph storage maintenance

On SNAP `com-Orkut` with 3,072,441 vertices and 234,370,166 directed arcs over 60 epochs, a wider bounded storage envelope reduces consolidations from 15 to 6 and total consolidation time from 386.573 s to 156.128 s, a **59.6% reduction**. Maintenance-amortized throughput increases from 19,135 to 43,062 operations/s (**2.25×**) while peak RSS rises by approximately **6.6%**.

This establishes a measured time/memory trade-off on one large graph; it does not claim that the 1.50× envelope is universally optimal.

### 6.10 Supporting breadth

A three-dataset triangle-crossover campaign retains five repetitions at 13 update fractions per dataset and exactness against full recomputation. Dataset-specific crossover behavior appears outside the primary BFS experiment as well. Hosted 4-thread and compression results remain supporting implementation evidence only and are not used to make many-core or universal-performance claims.

## 7. Discussion

### 7.1 Architecture and selector quality are different claims

The new held-out campaign sharpens the paper's thesis. The architectural claim is that exact repair and exact recomputation should be exposed as alternative physical plans over a shared mutable substrate. The policy claim is narrower: one current selector can often exploit that crossover, but its quality is workload-dependent. `Amazon0312` and `CollegeMsg` make this distinction explicit.

This separation is useful because selector improvement does not require rewriting the exact maintenance algorithm. New preflight features or a learned policy can be evaluated under the same execution interface, oracle, and fallback contract.

### 7.2 Pre-repair selection can avoid real fallback work

The production 0.35 campaign closes an important evidentiary gap. The benefit of pre-repair placement is no longer inferred from zero fallback in an isolation harness; it is directly measured against a fallback-only path. All six observed fallback events occur in the declared cascade stress case, and pre-repair selection avoids all six plus 17.323 ms of conservatively measured double work. The real-graph rows trigger no 0.35 fallback, so the experiment establishes the mechanism rather than its natural frequency. Separately, selector+fallback reduces cumulative answer-ready latency by 42.7% across all retained observations and 42.2% on the real-graph subset. The artifact's 33 `false_full` labels are explicit-full/non-fallback labels, not wrong-arm assertions.

### 7.3 Uncertainty matters; not every feature does

The clean ablation shows that uncertainty handling is essential in the current policy and that structural preflight is useful. It also shows that previous-affected-work is not independently justified by this campaign. That negative result is retained. The appropriate response is to simplify future selector design or re-evaluate the feature on broader frozen workloads, not to declare every existing heuristic necessary.

### 7.4 Negative results are part of the result

The benchmark record intentionally retains several reversals: recomputation wins when repair becomes broad; NetworKit wins the evaluated `ca-GrQc` workload; GAP wins weighted SSSP; GraphBolt wins the largest matched update fraction; RisGraph remains faster on its documented hosted campaign; and the current selector performs poorly on part of the timestamp-ordered `CollegeMsg` holdout. Removing these cases would weaken the systems paper because its central claim is precisely that execution preference is workload-dependent.

### 7.5 Storage and algorithm adaptation are related but distinct

The storage policy and analytical selector embody the same broad principle—avoid global work while local state remains economical—but operate at different layers and timescales. We do not collapse them into one learned policy. Storage consolidation is a bounded maintenance decision; repair-versus-recompute selection is an answer-ready execution decision.

### 7.6 Generalization beyond BFS

The architecture does not require every analytic to use the same features or thresholds. An exact triangle counter or k-core maintainer exposes different dependency state, and PageRank uses a tolerance-based numerical contract. What transfers is the interface: a localized arm, a global arm, pre-execution observables, explicit fallback where appropriate, and post-execution telemetry. BFS is therefore the detailed exact case study rather than evidence that one selector is universal.

## 8. Limitations

The primary comparative evidence is hosted and intentionally scoped. The paper does not claim stable many-core scaling, multi-socket NUMA behavior, hardware-counter advantages, NVMe/out-of-core superiority, or universal peak throughput. NetworKit evidence covers one thread and two graph families. The accepted RisGraph and NetworKit campaigns were executed separately, so their absolute latencies are not combined into a three-system ranking.

The primary three-graph selector campaign uses a historical fixed graph/root program. Its low average regret is therefore not a fresh generalization result. The frozen held-out campaign improves the evidence but also exposes a substantial failure on timestamp-ordered `CollegeMsg`: 34.52% equal-regime mean regret and a very large worst-regime tail. We consequently make no uniform near-oracle or universal selector-generalization claim.

`CollegeMsg` preserves observed interaction arrival order, but repeated interactions are idempotent under simple-graph semantics and sliding-window removals are induced expiries rather than observed deletion events. It is therefore a genuine temporal-order test of the selector, not a perfect model of every real temporal graph semantics.

The production 0.35 fallback campaign directly measures avoided repair-then-full work, but all six internal fallback events occur in the 220K destructive-cascade mechanism stress case; the 72 real-graph observations have zero 0.35 fallbacks. The experiment therefore establishes that the architecture can avoid measured double work when such a case occurs, not the population frequency of such cases in natural workloads. The reported 42.7% all-observation and 42.2% real-graph cumulative latency reductions are scoped to this retained campaign.

The clean ablation is stronger than the historical development record because it changes one mechanism at a time, but it is still tied to one frozen campaign. Structural preflight and uncertainty are supported there; previous-affected-work is not. Future policy work should treat this as evidence to simplify or revisit the feature rather than as a final causal model.

The GraphBolt comparison uses a pinned legacy artifact runtime on a matched hosted allocation. It provides a serious same-stream reference with a retained winner reversal, but it is not a universal evaluation of every modern GraphBolt/DZiG configuration.

Adaptive selection is studied most deeply for BFS. Some destructive weighted-SSSP updates conservatively recompute, and PageRank uses residual/tolerance validation rather than the exact BFS contract. The storage result demonstrates a clear canonicalization trade-off on one very large graph but does not determine a universally optimal envelope.

## 9. Related work

### 9.1 Dynamic and incremental graph processing

GraphIn introduced a property-based dual-path execution model that can switch between incremental and static processing. VeloGraphX therefore does not claim that the existence of two graph-processing paths is new. The distinction here is the exact repair/recompute framing over one mutable analytical state, the pre-repair placement of the decision, and explicit accounting for wrong-arm and repair-to-full work.

Bok et al. predict incremental-processing cost from prior history and select between incremental and static execution. History-based cost selection is therefore also prior art. VeloGraphX combines cost history with structural preflight and uncertainty while separating selector behavior from fallback and correctness; the contribution is the integrated exact systems design and evaluation rather than the general idea of using past cost.

GraphBolt and DZiG develop dependency-driven and sparsity-aware incremental graph processing. Their work motivates exploiting affected state rather than rerunning a whole computation. VeloGraphX's BFS repair likewise uses dependency structure, but this paper focuses on **when** that repair should be selected versus recomputation. The matched GraphBolt artifact experiment is reported as a scoped reference rather than as a claim to reproduce or rank the full published DZiG/GraphBolt design space.

RisGraph targets low-latency evolving-graph processing and provides an important external dynamic-system reference. Layph addresses broad propagation in dynamic graph computation through layered processing. These systems reinforce that dynamic performance depends on how change propagates; VeloGraphX studies the complementary physical-plan selection problem under exact repair/recompute alternatives.

### 9.2 Mutable graph storage

Dynamic storage itself is not claimed as novel. GraphOne uses hybrid representations for graph updates and analytical views, while Teseo develops a sophisticated mutable representation with transactional support. VeloGraphX's segmented CSR, packed deltas, and sparse row patches should be read as the substrate required to make repeated repair and explicit consolidation practical, not as a claim to have invented mutable graph storage.

The storage contribution is consequently scoped to its interaction with execution: updates need not force canonical reconstruction, reverse adjacency supports dependency repair, and canonicalization is exposed as a bounded maintenance action whose cost can be measured separately.

### 9.3 Static graph analytics and sparse linear algebra

GAP Benchmark Suite provides optimized graph kernels and a widely used evaluation reference; LAGraph/SuiteSparse:GraphBLAS represents graph algorithms through sparse linear algebra. The static comparison is contextual rather than a claim that VeloGraphX replaces either ecosystem. It verifies that the recomputation arm is credible on BFS while retaining the weighted-SSSP case where GAP is substantially faster.

NetworKit is a mature graph-analysis library with dynamic capabilities and serves as a paired external BFS baseline. The observed winner reversal across `web-Google` and `ca-GrQc` is consistent with the paper's refusal to make a universal fastest-system claim.

## 10. Conclusion

VeloGraphX treats exact localized repair and full recomputation as alternative physical strategies for the same evolving-graph result. A mutable graph substrate makes repeated updates practical, exact BFS maintenance exposes localized work, and a pre-repair selector can choose full execution before entering repair while retaining fallback as a safety/performance bound.

The expanded evaluation strengthens and narrows the paper at the same time. The primary selector remains exact with low average regret on its historical three-graph program. Under the real 0.35 fallback bound, pre-repair selection avoids all six retained fallback opportunities and 17.323 ms of conservatively measured repair-then-full work; all six fallbacks arise in the declared cascade stress case, while the real-graph subset has none. Across the retained replay, selector+fallback also lowers cumulative answer-ready latency by 42.7% overall and 42.2% on real graphs. A frozen held-out campaign performs well on unseen `Amazon0312` but poorly on timestamp-ordered `CollegeMsg`, demonstrating that the current selector is not universally general. Clean ablation supports structural preflight and uncertainty while refusing to assign causal credit to every feature. External comparisons retain workload-specific winner reversals.

The resulting systems lesson is therefore deliberately architectural rather than promotional: **incremental maintenance should be a selectable exact execution strategy, not an unconditional architectural assumption.**
