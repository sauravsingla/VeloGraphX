# Paper Claim-to-Evidence Map

This page maps candidate paper claims to the corresponding implementation, validation, and evidence boundary. It is intended to keep the manuscript aligned with what the repository actually establishes.

| Candidate claim | Implementation / contract | Current evidence | Publication boundary |
| --- | --- | --- | --- |
| VeloGraphX supports mutable graph analytics without canonical CSR reconstruction after every update batch. | `include/velographx/storage/`, segmented CSR, packed deltas, sparse patches, explicit consolidation. | Architecture and storage documentation; storage/canonicalization tests and A/B campaigns. | Performance generalization still depends on workload and hardware. |
| Dynamic BFS / unweighted SSSP can repair localized affected state while retaining an exact result. | `include/velographx/incremental/bfs.hpp` with insertion propagation, deletion-affected-region discovery, localized repair, and recomputation fallback. | Differential/randomized tests and retained dynamic-BFS campaigns. | Do not claim every update is repaired incrementally; fallback is part of the correctness contract. |
| The best exact execution strategy changes with update/workload regime. | Both localized repair and full recomputation are executable paths; update-fraction and adaptive-policy campaigns exercise the crossover. | Retained experiments include regimes where repair wins and regimes where full recomputation wins. | Dedicated controlled hardware and broader graph-family coverage strengthen any universal crossover characterization. |
| An adaptive policy can choose between repair and recomputation while preserving exactness. | `benchmarks/adaptive_policy_bfs.cpp` evaluates always-incremental, always-full, simple-threshold, and adaptive policies and checks each against fresh BFS. | Adaptive-policy campaigns plus the minimal reviewer reproduction path. | Report policy quality on the evaluated datasets/regimes; avoid universal optimality claims. |
| VeloGraphX supports maintained analytics beyond BFS. | Incremental/dynamic implementations for weighted SSSP, connected components, triangles, k-core, and PageRank-related workflows. | Unit/differential tests plus algorithm-specific benchmark campaigns. | Breadth demonstrates system generality; algorithm-specific performance claims require their own evidence. |
| The storage/runtime design is independent of one concrete graph representation. | Graph-access abstraction and adapters separate algorithms from storage representation. | `graph_access` tests and architecture documentation. | Interoperability is an engineering capability; performance parity across foreign representations is not implied. |
| VeloGraphX includes multicore, SIMD-oriented, NUMA-aware, compression, and partition-I/O mechanisms. | Runtime, kernel, memory, compression, and partition components under `include/velographx/`. | Component tests and hosted engineering campaigns. | True many-core/NUMA and research-scale I/O claims require dedicated controlled hardware. |
| VeloGraphX performance comparisons preserve exactness and explicit timing semantics. | Benchmark methodology defines input boundaries, update/application timing, validation separation, competitor revision pinning, and retained artifacts. | `docs/benchmark-methodology.md` and campaign-specific contracts. | A comparison is only publication-grade when the executed run satisfies its documented contract. |
| External systems can win on some workloads; VeloGraphX does not rely on win-only reporting. | Benchmark record retains negative results and workload-specific winners. | NetworKit/GAP/LAGraph and other retained competitor evidence where semantics match. | Cross-system rankings must not combine incompatible timing envelopes or different machines. |
| Hosted CI is useful for reproducible engineering evidence but is not controlled-hardware proof. | Explicit evidence-tier language throughout benchmark and limitations docs. | Hosted runs retain machine-readable artifacts and correctness gates. | Headline publication performance should come from dedicated, controlled, reproducible runs. |

## Primary implementation references

- [Architecture](architecture.md)
- [Dynamic storage design](dynamic-storage.md)
- [`../include/velographx/incremental/`](../include/velographx/incremental/)
- [`../benchmarks/adaptive_policy_bfs.cpp`](../benchmarks/adaptive_policy_bfs.cpp)
- [`../benchmarks/update_fraction_campaign.cpp`](../benchmarks/update_fraction_campaign.cpp)

## Primary evidence references

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

1. every quantitative statement in the paper should point to a retained artifact or reproducible command;
2. every performance comparison should state the timing boundary and machine class;
3. every dynamic result should include exactness/checksum validation;
4. negative results should remain visible when they define the crossover or claim boundary; and
5. hosted-CI results should not be promoted as dedicated-hardware claims.
