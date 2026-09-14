# VeloGraphX Novelty Ledger

This ledger prevents accidental novelty claims. Status values are: **known prior art**, **integration hypothesis**, **candidate novelty**, **validated contribution**, or **rejected**.

| Idea | Closest prior-art theme | VeloGraphX difference to test | Hypothesis / experiment | Status |
|---|---|---|---|---|
| CSR + mutable delta adjacency | Dynamic graph stores using base + update buffers/logs | Degree- and workload-aware representation plus measured compaction cost | Compare traversal/update/memory across update rates | known prior art / integration hypothesis |
| Adaptive compaction | LSM/log/delta graph stores and packed layouts | Cost model uses measured query penalty, mutation rate, fragmentation and memory pressure | Predict best compaction point vs fixed thresholds | candidate novelty pending literature review |
| Incremental vs full recompute runtime switch | GraphIn dual-path incremental/static execution; cost-model-based incremental/static selection; dynamic analytics systems | **The switch itself is prior art.** Candidate contribution is a pre-repair, per-batch exact execution policy that uses online graph/update/algorithm-state signals, avoids repair→full double work, and is evaluated against a per-batch oracle with tail regret and selector cost | Compare always-full, always-incremental, simple dual-path heuristic, history-only cost model, VeloGraphX selector, and oracle under identical streams/hardware | known prior art for switching; narrowed candidate novelty for pre-repair policy/evaluation |
| Adaptive strategy inside incremental processing | DZiG sparsity-aware adaptive incremental refinement and related adaptive incremental runtimes | VeloGraphX chooses between localized exact repair and fresh full recomputation rather than only switching among incremental refinement modes | Same-stream comparison of policy outcomes, decision timing and wasted-work avoidance | known prior art / differentiation required |
| Affected-region execution | Incremental graph analytics; GraphBolt/KickStarter-style dependency/repair techniques | Shared runtime abstraction across several algorithms | Compare reusable runtime vs algorithm-specific implementations | known prior art / integration hypothesis |
| Pre-repair crossover selection | GraphIn property-based dual path; Bok et al. history-based incremental/static cost model | Decide before expensive affected-region discovery/repair is committed, using update fraction, graph scale, reachable/root state, prior affected work, observed costs and uncertainty; selector owns full recomputation to prevent hidden repair→full double work | Measure wrong-arm rate, selector overhead, mean/p95/p99/worst oracle regret, and avoided double-work events | **candidate novelty pending exhaustive literature review and baseline reconstruction** |
| Oracle-relative execution-policy evaluation | Online policy/oracle evaluation in systems and scheduling literature | Apply per-batch oracle regret, tail regret and worst-regime regret to exact dynamic graph repair-vs-recompute selection, with decision cost included | Pre-register metrics and report all regimes including negative results | integration hypothesis / candidate evaluation contribution |
| Adaptive intersection dispatcher | Linear, galloping, SIMD and bitmap intersections | Runtime selection using sizes, density, representation and calibrated CPU crossover | Microbenchmark across degree distributions and CPUs | integration hypothesis |
| NUMA-aware scheduling | GraphIt and CPU graph systems | Couple NUMA locality with dynamic affected regions rather than static full-graph traversal | Measure remote traffic and latency on multisocket hardware | integration hypothesis |
| Incremental triangle counting | Dynamic triangle algorithms | Reuse common-neighbor kernels and dynamic storage representation | Compare to full recount across insert/delete traces | known prior art |
| Incremental PageRank | Delta/residual PageRank work | Integrate runtime crossover and architecture-aware frontier scheduling | Compare latency, touched edges and convergence work | known prior art / integration hypothesis |
| Memory-budgeted NVMe mode | Out-of-core graph systems | Preserve versioned dynamic updates with bounded resident set | Measure latency and I/O amplification under fixed RAM | candidate novelty pending later review |

## Prior art that constrains the repair-vs-recompute claim

The following papers must be discussed before making any priority claim around adaptive incremental execution:

- **GraphIn** — D. Sengupta et al., “GraphIn: An Online High Performance Incremental Graph Processing Framework,” Euro-Par 2016, pp. 319–333, DOI: `10.1007/978-3-319-43659-3_24`. GraphIn includes a property-based dual-path execution model that chooses incremental or static computation. Therefore, VeloGraphX must not claim invention of incremental-vs-static switching.
- **Cost Model Based Incremental Processing in Dynamic Graphs** — K. Bok et al., *Electronics* 11(4):660, 2022, DOI: `10.3390/electronics11040660`. This work predicts detection/processing cost from past history and selects incremental or static processing. Therefore, VeloGraphX must not claim invention of history-based cost selection.
- **DZiG** — M. Mariappan, J. Che, K. Vora, “DZiG: Sparsity-Aware Incremental Processing of Streaming Graphs,” EuroSys 2021. DZiG adapts how incremental computation is performed as sparsity changes. Therefore, “adaptive incremental execution” is not a novelty claim by itself.
- **Ingress** — S. Gong et al., “Automating Incremental Graph Processing with Flexible Memoization,” PVLDB 14(9), 2021, DOI: `10.14778/3461535.3461550`. Ingress automatically selects among memoization policies for incrementalization; VeloGraphX must distinguish execution-arm selection from memoization-policy selection.
- **GraphBolt** — M. Mariappan and K. Vora, “GraphBolt: Dependency-Driven Synchronous Processing of Streaming Graphs,” EuroSys 2019, DOI: `10.1145/3302424.3303974`. GraphBolt establishes dependency-driven incremental propagation under BSP semantics.
- **KickStarter** — K. Vora, R. Gupta, G. Xu, “KickStarter: Fast and Accurate Computations on Streaming Graphs via Trimmed Approximations,” ASPLOS 2017, DOI: `10.1145/3037697.3037748`. KickStarter establishes selective repair/trimming of state after graph changes for monotonic computations.
- **RisGraph** — G. Feng et al., “RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s,” SIGMOD 2021, DOI: `10.1145/3448016.3457263`. RisGraph is strong prior art for exact low-latency dynamic graph processing, safe/unsafe update handling and dynamic scheduling.
- **Layph** — S. Yu et al., “Layph: Making Change Propagation Constraint in Incremental Graph Processing by Layering Graph,” ICDE 2023. Layph attacks the same broad pathology—small graph changes causing large propagation—by structurally constraining propagation rather than selecting fresh recomputation.

## Candidate research claim after prior-art tightening

The strongest currently defensible hypothesis is **not** “VeloGraphX decides whether to run incrementally.” That is prior art.

A candidate claim, subject to exhaustive literature review and experiment, is:

> VeloGraphX formulates exact dynamic graph execution as a **pre-repair per-batch policy problem**: before expensive localized-repair discovery becomes sunk cost, the runtime uses observable graph, update, algorithm-state and historical-cost signals to choose localized repair or fresh full recomputation; the policy explicitly owns recomputation to avoid repair→full double work and is evaluated by per-batch oracle regret, including tail and worst-regime behavior, with selector overhead included.

Every part of that sentence must be supported independently. If literature reveals the same mechanism and evaluation formulation, the claim must be narrowed again.

## Required publication experiment

The policy paper path requires one same-run experiment with identical graph state, root/query, update stream, build, thread placement and hardware for:

1. `always_full`;
2. `always_incremental`;
3. `simple_threshold` — a deliberately simple dual-path heuristic, **not** claimed as a faithful GraphIn reproduction;
4. `history_cost_model` — a documented reconstruction inspired by history-based cost selection, **not** claimed as the original authors’ implementation unless their code is used;
5. `adaptive` — the frozen VeloGraphX selector;
6. an offline per-batch oracle `min(full, incremental)` used only for evaluation.

For every policy report answer-ready latency, exactness, selector cost, fraction of full recomputations, wrong-arm rate relative to the oracle, mean/median/p95/p99/max regret and worst-regime regret. Any repair attempt that subsequently falls back to full recomputation must be reported as double work rather than hidden inside an aggregate.

## Required record for future claims

For each proposed contribution document: idea, closest paper/system, publication date, public implementation, what it solves, limitations, VeloGraphX difference, falsifiable hypothesis, experiment, measured result, and final novelty status.

## Rule

Implementation novelty is not research novelty. A feature remains prior art unless the project demonstrates a meaningful new mechanism or empirically new systems result. Do not use “first”, “first-ever”, “novel incremental-vs-full switching”, or equivalent wording without a completed literature review that explicitly clears GraphIn, Bok et al., DZiG and later work.