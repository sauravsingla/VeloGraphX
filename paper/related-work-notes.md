# Related-work and novelty notes

This file records the prior work that most directly constrains the VeloGraphX novelty claim. It is a manuscript aid, not a finished bibliography. Primary/authoritative metadata should be rechecked when the venue bibliography is frozen.

## GraphIn — dual-path incremental/static execution

**Dipanjan Sengupta, Narayanan Sundaram, Xia Zhu, Theodore L. Willke, Jeffrey S. Young, Matthew Wolf, Karsten Schwan.** “GraphIn: An Online High Performance Incremental Graph Processing Framework.” Euro-Par 2016, pp. 319–333. DOI: `10.1007/978-3-319-43659-3_24`.

GraphIn is especially important to the novelty boundary because it explicitly introduces a property-based **dual-path execution** model that can choose between incremental and static computation when incremental work becomes unattractive.

**Consequence for VeloGraphX:** do not claim invention of switching between incremental execution and static/full recomputation. The manuscript must explain how VeloGraphX differs in the decision contract, signals, exact answer-ready comparison, pre-repair selection/double-work avoidance, mutable substrate, and evaluation.

## Bok et al. — history-based cost model

**Kyoungsoo Bok, Jungkwon Cho, Hyeonbyeong Lee, Dojin Choi, Jongtae Lim, Jaesoo Yoo.** “Cost Model Based Incremental Processing in Dynamic Graphs.” *Electronics* 11(4):660, 2022. DOI: `10.3390/electronics11040660`.

This work predicts incremental detection/processing cost from past processing history and selects incremental or static processing accordingly.

**Consequence for VeloGraphX:** do not claim invention of history/cost-based incremental-versus-static selection. Preserve the `history_cost_model` policy in the publication harness as a conceptual baseline. Emphasize the current policy's graph/root/scale signals, exact two-arm oracle framing, measured tail behavior, and explicit prevention of repair-then-full double work.

## GraphBolt — dependency-driven exact/incremental processing

**Mugilan Mariappan, Keval Vora.** “GraphBolt: Dependency-Driven Synchronous Processing of Streaming Graphs.” EuroSys 2019. DOI: `10.1145/3302424.3303974`.

GraphBolt uses dependency tracking to propagate the effect of graph changes while retaining BSP-style synchronous semantics.

**Consequence for VeloGraphX:** dependency-driven affected-region processing is prior art. Localized repair should be presented as an enabling mechanism, not by itself as the paper's primary novelty.

## DZiG — sparsity-aware incremental processing

**Mugilan Mariappan, Joanna Che, Keval Vora.** “DZiG: Sparsity-Aware Incremental Processing of Streaming Graphs.” EuroSys 2021, pp. 83–98. DOI: `10.1145/3447786.3456230`.

DZiG extends dependency-driven streaming-graph processing with sparsity-aware incremental execution.

**Consequence for VeloGraphX:** avoid generic claims that adapting incremental work to sparsity or affected scope is new. The manuscript should compare execution semantics and emphasize the repair-versus-recompute choice before expensive repair work is paid.

## RisGraph — exact low-latency evolving-graph processing

**Guanyu Feng, Zixuan Ma, Daixuan Li, Shengqi Chen, Xiaowei Zhu, Wentao Han, Wenguang Chen.** “RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s.” SIGMOD 2021. DOI: `10.1145/3448016.3457263`.

RisGraph targets low-latency per-update analysis with localized data access and inter-update parallelism.

**Consequence for VeloGraphX:** do not position localized exact dynamic BFS as unique. Retain the accepted same-run RisGraph result even when RisGraph is faster; that comparison helps establish honest system context.

## Layph — constraining change propagation

**Song Yu, Shufeng Gong, Yanfeng Zhang, Wenyuan Yu, Qiang Yin, Chao Tian, Qian Tao, Yongze Yan, Ge Yu, Jingren Zhou.** “Layph: Making Change Propagation Constraint in Incremental Graph Processing by Layering Graph.” ICDE 2023, pp. 2766–2779. DOI: `10.1109/ICDE55515.2023.00212`.

Layph addresses the problem that small changes can cause graph-wide iterative work by introducing a layered representation that constrains expensive change propagation.

**Consequence for VeloGraphX:** the observation that small updates can induce large affected work is established. VeloGraphX should claim an execution-response design—choose an exact global path when local propagation is predicted to be uneconomical—not discovery of the propagation problem itself.

## GraphOne — evolving-graph storage and multiple access views

**Pradeep Kumar, H. Howie Huang.** “GraphOne: A Data Store for Real-time Analytics on Evolving Graphs.” FAST 2019, pp. 249–263.

GraphOne combines complementary edge-list and adjacency-list storage and uses dual versioning/GraphView abstractions so evolving-graph ingestion and analytics can coexist.

**Consequence for VeloGraphX:** hybrid mutable storage for evolving graphs is established prior art. Segmented CSR, packed deltas, sparse row patches, and explicit consolidation should be presented as VeloGraphX's enabling substrate for repeated exact analytics and the repair/recompute execution study, not as a generic first dynamic-graph store.

## Teseo — transactional dynamic structural graph storage

**Dean De Leo, Peter Boncz.** “Teseo and the Analysis of Structural Dynamic Graphs.” PVLDB 14(6):1053–1066, 2021. DOI: `10.14778/3447689.3447708`.

Teseo provides main-memory dynamic structural graph storage with transactional support using sparse arrays and fat trees, emphasizing update robustness and analytical scans.

**Consequence for VeloGraphX:** mutable graph representation and update/scan trade-offs are mature research territory. The paper's storage contribution should stay tied to the bounded canonicalization mechanism and to supporting exact execution alternatives rather than claiming a novel storage category.

## Defensible VeloGraphX novelty framing

A conservative framing suitable for reviewer scrutiny is:

> VeloGraphX treats localized repair and full recomputation as competing exact physical execution strategies over a common mutable graph substrate, and studies a pre-repair selector that uses structural state and observed cost to avoid both unnecessary global recomputation and repair-discovery work that would subsequently fall back to full execution.

The novelty case becomes stronger when supported by all of the following rather than by any one mechanism in isolation:

1. exact two-arm execution semantics under one system substrate;
2. pre-repair selection rather than only an internal fallback after repair work has begun;
3. explicit telemetry for internal fallback/double work, wrong-arm selection, tail regret, and decision overhead;
4. crossover characterization that retains regimes where full recomputation wins;
5. external comparisons that retain competitor wins; and
6. reproducible, checksum-pinned artifact contracts.

## Claims to avoid

Do not state that VeloGraphX is the first system to:

- switch between incremental and static/full execution;
- use graph properties to select incremental/static execution;
- use past cost history to predict incremental execution cost;
- perform dependency-driven or affected-region incremental graph processing;
- adapt graph processing to sparsity/change propagation;
- provide a mutable store for evolving graph analytics; or
- provide low-latency exact analytics over evolving graphs.

Any “first” claim would require a broader systematic literature review than the evidence above and is unnecessary for the paper's strongest contribution.
