# Related-work positioning

VeloGraphX should be positioned as an **exact CPU-native dynamic-graph system that couples compact mutable storage, localized repair, and a pre-repair workload-aware choice between incremental repair and fresh recomputation**. The contribution is the system integration, decision mechanism, and measured execution policy; it is not a claim that BFS, SSSP, triangles, connected components, k-core, PageRank, incremental graph processing, adaptive execution, or the existence of an incremental-vs-static switch are individually new.

## Comparison matrix

| System / work | Primary contribution | Dynamic execution model | Why it is important prior art | VeloGraphX distinction that must be demonstrated |
| --- | --- | --- | --- | --- |
| **GraphIn** (Euro-Par 2016) | Online incremental graph framework and I-GAS programming model | Property-based dual-path execution chooses incremental or static computation | Directly invalidates any claim that VeloGraphX invented incremental-vs-static switching | Pre-repair per-batch policy driven by online graph/update/algorithm-state/cost signals, explicit prevention of repair→full double work, and oracle/tail-regret evaluation |
| **KickStarter** (ASPLOS 2017) | Trims unsafe/unprofitable approximations after deletions | Selective state repair for monotonic computations | Establishes profitable selective repair and the observation that reusing prior state can be slower than fresh computation | VeloGraphX chooses between exact localized repair and fresh full recomputation before expensive repair becomes sunk cost |
| **GraphBolt** (EuroSys 2019) | Dependency-driven incremental processing with BSP semantics | Tracks dependencies and propagates graph-change effects through intermediate values | Strong prior art for affected-region/dependency-driven incremental processing | VeloGraphX does not claim affected-region repair as new; its candidate contribution is execution-arm selection around exact repair vs fresh recomputation |
| **Ingress** (PVLDB 2021) | Automatically incrementalizes vertex-centric algorithms and selects among four memoization policies | Chooses memoization/incrementalization policy from algorithm properties | Strong prior art for automated incrementalization and policy selection | VeloGraphX studies online per-batch execution-arm selection rather than choosing a memoization representation/policy for an algorithm |
| **DZiG** (EuroSys 2021) | Sparsity-aware incremental refinement | Adaptively changes the manner of incremental computation as sparsity evolves | Directly invalidates a generic “adaptive incremental execution” novelty claim | VeloGraphX selects between localized repair and fresh recomputation and evaluates decision quality against an offline oracle |
| **RisGraph** (SIGMOD 2021) | Real-time evolving-graph processing with very low per-update latency | Incremental processing with safe/unsafe update classification and latency-aware scheduling | Closest systems comparison for exact low-latency CPU dynamic graph processing | VeloGraphX focuses on batched repair/recompute crossover, selector cost modelling, and same-semantics policy-regret evaluation |
| **Bok et al. cost model** (*Electronics*, 2022) | Uses historical statistics to predict incremental and static processing cost | Selects incremental or static processing based on estimated recalculation-region detection/processing cost | Directly invalidates any claim that history-based repair-vs-static cost selection is new | VeloGraphX must show materially richer pre-repair signals/mechanism, uncertainty handling, prevention of double work, broader exactness semantics, and stronger oracle/tail-regret evaluation |
| **Layph** (ICDE 2023) | Constrains change propagation through a layered graph representation | Limits costly global incremental propagation to a skeleton and affected subgraphs | Addresses the same fundamental pathology: small changes can trigger near-global incremental work | VeloGraphX responds by selecting fresh recomputation when predicted repair cost is unfavorable rather than structurally constraining propagation |
| **Aspen** (PPoPP 2019) | Low-latency graph streaming with compressed purely-functional trees | Dynamic updates with concurrent readers | Important storage-oriented comparison | VeloGraphX uses a different mutable-storage design and couples storage state to exact repair/recompute policy |
| **GraphDelta** (JSA 2026) | Distributed incremental framework combining inter-batch and intra-batch optimization | Reuses historical results between batches and selectively updates active vertices within a batch | Recent evidence that multi-level incremental execution remains active prior art | VeloGraphX is CPU-native and focuses on exact localized repair versus fresh recomputation rather than GraphX-based distributed execution |

## Reviewer-facing distinction

The following claims are **not defensible novelty claims** for VeloGraphX:

- “first incremental dynamic graph system”;
- “first adaptive incremental graph system”;
- “first system to choose incremental processing or recomputation”;
- “first history-based cost model for incremental-vs-static graph processing”;
- novelty for dependency-driven/affected-region repair itself;
- novelty for the individual graph algorithms.

The candidate research contribution is narrower:

> **VeloGraphX treats exact localized repair versus fresh recomputation as a pre-repair per-batch execution-policy problem. Before expensive affected-region discovery/repair becomes sunk cost, the runtime combines graph scale, update characteristics, maintained algorithm state, prior affected work, observed execution costs and uncertainty to select an execution arm. The selector explicitly owns recomputation to avoid hidden repair→full double work, and policy quality is evaluated against a same-run per-batch oracle using mean and tail regret with selector overhead included.**

This is a **candidate** novelty statement, not a priority claim. A broader literature review may require further narrowing.

## Why the timing of the decision matters

A dynamic runtime can make a nominally correct repair-vs-recompute decision too late. If it first discovers a large affected region, performs substantial repair work, and only then falls back to a full recomputation, answer-ready latency contains both costs:

`repair discovery/work + full recomputation`.

VeloGraphX should therefore distinguish **pre-repair selection** from **post-discovery fallback**. The paper must measure how often a baseline pays this double-work path and whether selector-owned recomputation avoids it. This distinction is more defensible than claiming the incremental/static choice itself as new.

## Required experimental questions

A systems paper should separate at least four questions:

1. **Storage/update efficiency:** what does the mutable representation cost under identical update streams?
2. **Exact dynamic algorithm efficiency:** when does localized repair beat fresh recomputation, and where is the crossover?
3. **Policy quality:** how closely does a runtime selector track the per-batch oracle, including p95/p99/max regret, wrong-arm rate, selector overhead, and full-recompute frequency?
4. **Decision timing / wasted work:** how much answer-ready latency is lost to repair discovery or partial repair before a full fallback, and how much of that is avoided by pre-repair selection?

Absolute timings from different machines or incompatible semantics must not be used to construct a synthetic ranking. Same-run comparisons should be preferred; otherwise results must be labelled as separate campaigns.

## Baseline discipline

The policy experiment should include:

- `always_full`;
- `always_incremental`;
- `simple_threshold`, described only as a simple dual-path heuristic and **not** as a GraphIn reproduction;
- `history_cost_model`, documented as a methodological reconstruction inspired by prior history-based cost selection and **not** as the original authors’ implementation unless their source is actually used;
- the frozen VeloGraphX `adaptive` selector;
- an offline per-batch oracle used only for evaluation.

If original open-source implementations of a relevant system can be run with compatible semantics, prefer them over reconstructed baselines. Reconstruction assumptions must be documented adjacent to the result.

## Primary references

- D. Sengupta, N. Sundaram, X. Zhu, T. L. Willke, J. S. Young, M. Wolf, K. Schwan, “GraphIn: An Online High Performance Incremental Graph Processing Framework,” Euro-Par 2016, pp. 319–333. DOI: `10.1007/978-3-319-43659-3_24`.
- K. Vora, R. Gupta, G. Xu, “KickStarter: Fast and Accurate Computations on Streaming Graphs via Trimmed Approximations,” ASPLOS 2017. DOI: `10.1145/3037697.3037748`.
- M. Mariappan, K. Vora, “GraphBolt: Dependency-Driven Synchronous Processing of Streaming Graphs,” EuroSys 2019. DOI: `10.1145/3302424.3303974`.
- M. Mariappan, J. Che, K. Vora, “DZiG: Sparsity-Aware Incremental Processing of Streaming Graphs,” EuroSys 2021.
- S. Gong et al., “Automating Incremental Graph Processing with Flexible Memoization,” PVLDB 14(9), 2021. DOI: `10.14778/3461535.3461550`.
- G. Feng et al., “RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s,” SIGMOD 2021. DOI: `10.1145/3448016.3457263`.
- K. Bok, J. Cho, H. Lee, D. Choi, J. Lim, J. Yoo, “Cost Model Based Incremental Processing in Dynamic Graphs,” *Electronics* 11(4):660, 2022. DOI: `10.3390/electronics11040660`.
- S. Yu et al., “Layph: Making Change Propagation Constraint in Incremental Graph Processing by Layering Graph,” ICDE 2023, pp. 2766–2779.
- L. Dhulipala et al., “Low-Latency Graph Streaming Using Compressed Purely-Functional Trees,” PPoPP 2019.
- “GraphDelta: A distributed incremental framework for efficient dynamic graph computing in edge intelligence,” *Journal of Systems Architecture* 176, 2026, 103834. DOI: `10.1016/j.sysarc.2026.103834`.

## Claim discipline

Use “system contribution”, “architecture”, “exact dynamic execution”, “pre-repair execution policy”, “repair/recompute crossover”, “oracle-relative policy quality”, and “workload-aware selection”. Avoid “first”, “first-ever”, “novel incremental-vs-full switching”, and claims of novelty for incremental processing, graph streaming, affected-region repair, memoization, adaptive incremental execution, history-based cost selection, or the individual graph algorithms unless a dedicated literature review supports the stronger wording.