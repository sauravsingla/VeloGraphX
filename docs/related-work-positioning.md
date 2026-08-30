# Related-work positioning

VeloGraphX should be positioned as an **exact CPU-native dynamic-graph system that couples compact mutable storage, localized repair, and workload-aware incremental-versus-recomputation control**. The contribution is the system integration and measured execution policy; it is not a claim that BFS, SSSP, triangles, connected components, k-core, PageRank, incremental graph processing, or adaptive execution are individually new.

## Comparison matrix

| System | Primary contribution | Dynamic execution model | Storage/runtime emphasis | Relation to VeloGraphX |
| --- | --- | --- | --- | --- |
| Ingress (PVLDB 2021; VLDB Journal 2024) | Automatically incrementalizes vertex-centric batch algorithms and selects among four memoization policies | Message-driven differentiation and automatic memoization-policy selection | Distributed vertex-centric runtime; memory-aware memoization | Strong prior art for automated incrementalization and policy selection. VeloGraphX instead studies exact CPU-native localized repair coupled to mutable storage and runtime incremental-vs-full recomputation decisions. |
| KickStarter | Dependency-driven incremental graph computation | Tracks dependencies to restrict recomputation after updates | Dependency-maintenance oriented | Establishes dependency-driven incremental processing; VeloGraphX should not claim localized incremental repair itself as new. |
| GraphBolt | Dependency-driven incremental processing | Refines memoized dependencies iteration by iteration | Incremental dependency maintenance | Relevant prior art for affected-region computation. VeloGraphX differs in storage/runtime integration and explicit incremental-versus-full execution control. |
| RisGraph (SIGMOD 2021) | Real-time evolving-graph processing with low tail latency and high throughput | Incremental processing with safe/unsafe update classification and latency-aware scheduling | Indexed adjacency lists, localized access, inter-update parallelism | Closest systems comparison for low-latency CPU dynamic graph processing. VeloGraphX differs in exact batched repair/recompute crossover, selector cost modelling, and reproducible same-semantics policy evaluation. |
| Aspen | Low-latency graph streaming with compressed purely-functional trees | Dynamic updates with concurrent readers | Compressed functional-tree graph representation | Important storage-oriented comparison. VeloGraphX uses a different mutable-storage design and couples it directly to exact repair and execution-policy selection. |
| GraphDelta (JSA 2026) | Distributed incremental dynamic graph framework combining inter-batch and intra-batch optimization | Reuses historical results between batches and selectively updates active vertices within a batch | GraphX/distributed edge-intelligence setting | Strong recent evidence that multi-level incremental execution is active prior art. VeloGraphX is CPU-native and focuses on exact repair versus recomputation decisions rather than GraphX-based distributed execution. |

## Reviewer-facing distinction

The broad question “incremental update or recompute?” is not itself new. Likewise, dependency-driven repair, memoization, safe/unsafe update classification, streaming graph storage, and selective active-vertex execution all have substantial prior work.

The defensible VeloGraphX research contribution is narrower:

> VeloGraphX integrates compact mutable graph storage, exact localized dynamic repair, affected-work measurement, and a workload-aware incremental-versus-full-recomputation controller in a CPU-native system, then evaluates that coupling under checksum-pinned same-semantics dynamic workloads with independent exactness verification and explicit average/tail regret measurements.

This wording deliberately avoids “first”, “first-ever”, and claims of novelty for individual algorithms.

## What should be compared experimentally

A systems paper should separate three questions:

1. **Storage/update efficiency:** cost of maintaining the mutable graph representation under identical update streams.
2. **Exact dynamic algorithm efficiency:** answer-ready latency for incremental repair versus full recomputation and external exact baselines.
3. **Policy quality:** how close the runtime selector stays to the best measured execution arm, including p95/worst-regime behavior and selector overhead.

Absolute timings from different machines or incompatible semantics must not be used to construct a synthetic ranking. Same-run comparisons should be preferred; otherwise results should be labelled as separate campaigns.

## Primary references

- Ingress: Shufeng Gong et al., “Automating Incremental Graph Processing with Flexible Memoization,” PVLDB 14(9), 2021. DOI: 10.14778/3461535.3461550.
- RisGraph: Guanyu Feng et al., “RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s,” SIGMOD 2021. DOI: 10.1145/3448016.3457263.
- Aspen: Laxman Dhulipala et al., “Low-Latency Graph Streaming Using Compressed Purely-Functional Trees,” PPoPP 2019.
- GraphDelta: “GraphDelta: A distributed incremental framework for efficient dynamic graph computing in edge intelligence,” Journal of Systems Architecture 176, 2026, 103834. DOI: 10.1016/j.sysarc.2026.103834.
- KickStarter and GraphBolt are dependency-driven incremental systems discussed directly in the Ingress related-work comparison; the paper bibliography should cite their original publications rather than relying only on secondary descriptions.

## Claim discipline

Use “system contribution”, “architecture”, “integration”, “exact dynamic execution”, and “workload-aware selection”. Avoid claiming that incremental processing, graph streaming, affected-region repair, memoization, or the individual graph algorithms are novel. Any stronger priority claim requires a dedicated literature review beyond this comparison matrix.
