# Paper Claim-to-Evidence Map

This page maps candidate paper claims to the corresponding implementation, validation, and evidence boundary. It is intended to keep the manuscript aligned with what the repository actually establishes.

The manuscript-facing run/artifact registry is [`paper-evidence-index.md`](paper-evidence-index.md), with a machine-readable mirror in [`../benchmarks/paper-evidence.json`](../benchmarks/paper-evidence.json).

| Candidate claim | Implementation / contract | Current evidence | Publication boundary |
| --- | --- | --- | --- |
| VeloGraphX supports mutable graph analytics without canonical CSR reconstruction after every update batch. | `include/velographx/storage/`, segmented CSR, packed deltas, sparse patches, explicit consolidation. | Architecture and storage documentation; storage/canonicalization tests and A/B campaigns. | Performance generalization still depends on workload and hardware. |
| Dynamic BFS / unweighted SSSP can repair localized affected state while retaining an exact result. | `include/velographx/incremental/bfs.hpp` with insertion propagation, deletion-affected-region discovery, localized repair, and recomputation fallback. | Differential/randomized tests and retained dynamic-BFS campaigns. | Do not claim every update is repaired incrementally; fallback is part of the correctness contract. |
| The best exact execution strategy changes with update/workload regime. | Both localized repair and full recomputation are executable paths; update-fraction and adaptive-policy campaigns exercise the crossover. | Retained experiments include regimes where repair wins and regimes where full recomputation wins. | Report this as a characterization of the evaluated graph/update regimes, not a universal crossover law. Dedicated hardware is not required for the scoped crossover claim when compared policies run in the same hosted campaign. |
| An adaptive policy can choose between repair and recomputation while preserving exactness. | `benchmarks/adaptive_policy_bfs.cpp` evaluates always-incremental, always-full, simple-threshold, and adaptive policies and checks each against fresh BFS. | Adaptive-policy campaigns plus the minimal reviewer reproduction path. | Report policy quality only on audited datasets/regimes. The final headline selector number remains gated on cross-referencing its retained run/artifact in the paper evidence index. |
| VeloGraphX supports maintained analytics beyond BFS. | Incremental/dynamic implementations for weighted SSSP, connected components, triangles, k-core, and PageRank-related workflows. | Unit/differential tests plus algorithm-specific benchmark campaigns. | Breadth demonstrates system generality; algorithm-specific performance claims require their own evidence. |
| The storage/runtime design is independent of one concrete graph representation. | Graph-access abstraction and adapters separate algorithms from storage representation. | `graph_access` tests and architecture documentation. | Interoperability is an engineering capability; performance parity across foreign representations is not implied. |
| VeloGraphX includes multicore, SIMD-oriented, NUMA-aware, compression, and partition-I/O mechanisms. | Runtime, kernel, memory, compression, and partition components under `include/velographx/`. | Component tests and hosted engineering campaigns. | Hosted evidence can support scoped 1–4-thread behavior. True many-core/NUMA, hardware-counter, microarchitecture-specific, and research-scale I/O claims require suitable dedicated/controlled hardware and are outside the core paper claim set. |
| VeloGraphX performance comparisons preserve exactness and explicit timing semantics. | Benchmark methodology defines input boundaries, update/application timing, validation separation, competitor revision pinning, and retained artifacts. | `docs/benchmark-methodology.md`, campaign-specific contracts, and the paper evidence index. | A comparison is manuscript-ready only when the executed run satisfies its documented contract and the statistic is reported consistently with the retained samples. |
| External systems can win on some workloads; VeloGraphX does not rely on win-only reporting. | Benchmark record retains negative results and workload-specific winners. | NetworKit/GAP/LAGraph/RisGraph and other retained competitor evidence where semantics match. | Cross-system rankings must not combine incompatible timing envelopes or separate hosted runners. Keep workload-specific winners visible. |
| Hosted CI can support reproducible relative paper claims when comparison conditions are controlled. | Same-run/paired workflows pin competitors, datasets, roots, thread settings, timing envelopes, repetitions, and correctness gates. | Accepted Tier A/B campaigns are enumerated in `paper-evidence-index.md`. | Hosted results may support exactness, crossover, paired ratios, ablations, and scoped 1–4-thread behavior. They must not be promoted to universal peak-performance, many-core, multi-socket NUMA, hardware-counter, or NVMe claims. |

## Primary implementation references

- [Architecture](architecture.md)
- [Dynamic storage design](dynamic-storage.md)
- [`../include/velographx/incremental/`](../include/velographx/incremental/)
- [`../benchmarks/adaptive_policy_bfs.cpp`](../benchmarks/adaptive_policy_bfs.cpp)
- [`../benchmarks/update_fraction_campaign.cpp`](../benchmarks/update_fraction_campaign.cpp)

## Primary evidence references

- [Paper evidence index](paper-evidence-index.md)
- [`../benchmarks/paper-evidence.json`](../benchmarks/paper-evidence.json)
- [Benchmark methodology](benchmark-methodology.md)
- [Current limitations](limitations.md)
- [Ablation study](ablation-study.md)
- [Hosted native competitor evidence](hosted-native-competitors.md)
- [External dynamic baselines](external-dynamic-baselines.md)
- [Published exact triangle baseline](same-run-published-baseline.md)
- [Canonicalization A/B evidence](canonicalization-ab-evidence.md)
- [Canonical publication campaign](canonical-publication-campaign.md)
- [Controlled-hardware execution](controlled-hardware-execution.md)

## Suggested manuscript discipline

Use the table above as a pre-submission gate:

1. every quantitative statement in the paper should point to a run/artifact in the paper evidence index or to a reproducible command;
2. every performance comparison should state the timing boundary, machine class, thread count, and statistic;
3. every dynamic result should include exactness/checksum validation;
4. negative results should remain visible when they define the crossover or claim boundary;
5. same-run paired hosted evidence should be reported as scoped relative evidence, not as universal hardware performance;
6. absolute times from different hosted runners must not be combined into one ranking;
7. manuscript figures should prefer median plus dispersion when raw repetitions are available and must not relabel historical means as medians without recomputing from raw samples; and
8. many-core, true NUMA, hardware-counter, NVMe, and microarchitecture-specific claims remain optional future work rather than blockers for the adaptive-execution paper.
