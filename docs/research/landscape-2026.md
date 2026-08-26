# Dynamic Graph Processing Landscape — 2026

## Scope

VeloGraphX targets CPU-native analytics for evolving graphs. This note focuses on the 2024–2026 landscape around dynamic storage, incremental analytics, adaptive execution, SIMD, NUMA, and out-of-core graph processing.

## Recent literature signals

A 2026 TKDE survey, *A Comprehensive Survey on Dynamic Graph Processing: Storage and Analytics*, organizes the field around two coupled problems: maintaining changing graph structure and producing up-to-date analytics. It covers CPU and GPU approaches and explicitly identifies remaining research gaps. A 2025 survey, *Recent Advances in Efficient Dynamic Graph Processing*, likewise emphasizes that dynamic algorithms should update results after graph changes rather than recompute blindly. A 2025 IEEE Access landscape review highlights the growing importance of streaming graph workloads. These works reinforce the premise that update efficiency, storage mutability, and analytics must be designed together.

## Relevant system families

- **Static CPU graph frameworks:** GAPBS, GraphIt, NetworKit, igraph, SuiteSparse:GraphBLAS/LAGraph. These provide essential performance and correctness baselines but generally optimize around static or batched immutable representations.
- **Incremental / streaming systems:** Maiter, GraphIn, KickStarter, GraphBolt, TEGRA, and related work demonstrate delta propagation, dependency tracking, and trimmed or stateful recomputation.
- **Graph DSL/compiler approaches:** GraphIt shows the value of scheduling decisions such as direction optimization and NUMA-aware execution. Dynamic extensions and generated incremental algorithms motivate separating algorithm semantics from execution policy.
- **Dynamic storage research:** prior work explores packed/segmented layouts, log/delta structures, tombstones, chunked adjacency, versioned structures, and update-friendly compressed representations.
- **GPU dynamic processing:** extensive work exists, but VeloGraphX intentionally focuses on CPU-only operation and commodity or multisocket CPU hardware.

## Design lessons for VeloGraphX

1. Dynamic storage and dynamic analytics cannot be optimized independently.
2. Small update batches are the natural target for incremental execution, but the affected region can grow enough that recomputation becomes cheaper.
3. The runtime therefore needs an explicit incremental-vs-recompute policy, not a permanent incremental mode.
4. CSR remains a strong traversal representation, so a hybrid CSR + mutable delta layer is a credible starting point.
5. Graph workloads are frequently memory-bound; locality, degree skew, frontier representation, and NUMA placement matter as much as arithmetic throughput.
6. Neighborhood-heavy kernels such as sorted intersection are good SIMD targets, but adaptive algorithm selection is more important than forcing one ISA-specific implementation.
7. Correctness under deletions is substantially harder than insertion-only maintenance and should be staged carefully.
8. Benchmarking must measure graph update cost, algorithm update cost, full recomputation cost, memory overhead, and crossover behavior together.

## Closest prior-art themes

VeloGraphX should assume that the following concepts are not novel by themselves: CSR, delta storage, tombstones, packed adjacency, direction-optimizing BFS, push/pull traversal, sparse/dense frontiers, SIMD neighbor intersection, NUMA partitioning, work stealing, incremental PageRank, incremental connectivity, incremental shortest paths, incremental triangle counting, versioned graph snapshots, and out-of-core graph partitioning.

Potential contribution must therefore come from a precisely measured mechanism or integration: for example, a better runtime cost model, a degree/update-aware storage policy, a new compaction decision rule, a new adaptive kernel dispatcher, or a demonstrably effective combination of affected-region analytics with CPU architecture signals.

## Open research questions

- Can the runtime predict the incremental/full-recompute crossover accurately across graph families and algorithms?
- Can storage representation be selected per vertex or partition using degree, mutation rate, query frequency, and locality?
- Can delta compaction decisions be driven by measured traversal penalty rather than fixed thresholds?
- Can incremental work estimation incorporate frontier growth and cache/NUMA behavior without becoming too expensive itself?
- How much benefit remains from SIMD after algorithmic work reduction is applied?
- How should the engine degrade when updates affect a large fraction of the graph?
- Can an out-of-core CPU runtime preserve low incremental latency under a fixed memory budget?

## Research discipline

No mechanism is called novel until `novelty-ledger.md` identifies the closest prior work and an experiment demonstrates a material difference. Negative results are publishable project knowledge and should be retained.