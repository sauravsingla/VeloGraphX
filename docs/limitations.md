# Current limitations

VeloGraphX implements dynamic, incremental, CPU-optimization, interoperability, and reproducibility capabilities, but several areas still require broader evaluation or dedicated hardware. This document separates implemented engineering capability from publication-grade evidence.

## SIMD and architecture-specific kernels

Architecture-specific neighbor-intersection paths are implemented for AVX2, AVX-512, and ARM NEON, with runtime ISA detection and scalar fallback. Differential testing protects correctness.

The remaining limitation is evaluation coverage across dedicated x86/ARM systems, graph degree distributions, compilers, and microarchitectures. Hosted CI is engineering evidence, not proof of universal SIMD speedup.

## Incremental algorithm coverage

Localized or incremental update paths are implemented for BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core, and PageRank-related workflows. The engine can fall back to full recomputation when localized repair is unsafe or unlikely to be worthwhile.

The project therefore does not claim that every update is always repaired incrementally. The incremental/full crossover still needs broader characterization across graph families and update patterns.

## Weighted dynamic graphs

Weighted graph updates and weighted SSSP are implemented. Destructive weighted changes may require conservative full recomputation when a safe localized repair is unavailable. Broader public-dataset evaluation and external comparison remain future work.

## NUMA and multicore scaling

The engine includes topology discovery, affinity, first-touch behavior, NUMA-aware placement, local queues, work stealing, push/pull frontiers and adaptive scheduling.

True multi-socket NUMA behavior and stable many-core scaling have not yet been established at publication grade. Those claims require dedicated hardware, controlled pinning/frequency, remote-memory measurements and identical-hardware competitor runs.

## Out-of-core execution and compression

Out-of-core infrastructure includes partition files, mmap/fallback reads, asynchronous loading, bounded caching, readahead and optional Linux `io_uring` prefetch. Compression includes delta, variable-byte, blocked variable-byte and fixed-width adjacency coding.

Research-scale NVMe performance and universally optimal codec thresholds are not yet established. Both require dedicated hardware and broader workload calibration.

## Python interoperability

Python bindings support NumPy, SciPy CSR and Apache Arrow ingestion when optional dependencies are available. The API is suitable for experimentation but is not yet a frozen long-term compatibility surface.

## Competitor evaluation

The repository has same-semantics dynamic-BFS evidence against RisGraph and NetworKit 11.2.1 `DynBFS`. The NetworKit campaign is native C++, uses one OpenMP thread, runs five paired repetitions per dataset on the same hosted runner, and requires exact correctness plus nontrivial reachability before timing is accepted. See [`external-dynamic-baselines.md`](external-dynamic-baselines.md).

The latest accepted campaign is GitHub Actions `33295590400`, VeloGraphX commit `c05bfcb9fa071ccee487d186fe92fdad9ad3ef66`, artifact `9727429231`. It measured **39.661 ms vs 39.628 ms NetworKit on web-Google (1.001x paired VX/NK)** and **0.3054 ms vs 0.0753 ms on ca-GrQc (4.05x paired VX/NK)**. The small-graph storage-policy and packed-delta changes reduced VeloGraphX ca-GrQc mean latency from the prior clean 0.3884 ms to 0.3054 ms while retaining exact results. web-Google remains at same-run hosted-CI parity.

ca-GrQc therefore remains a material optimization gap. The two obvious fixed-overhead issues—premature percentage-only compaction and duplicate forward-delta lookup—have been reduced, so further substantial progress is likely to require deeper storage or mutation-path work rather than changing BFS semantics.

Important limits remain:

- hosted CI is not dedicated performance hardware;
- the campaign uses one thread and two public graph families;
- one deterministic root/workload is used per dataset;
- the RisGraph result is from a separate hosted runner and cannot be merged into an absolute three-system ranking; and
- publication-grade evidence still requires dedicated same-machine runs, more roots/seeds/update regimes, multicore scaling and hardware-counter analysis.

Teseo/GFE, Aspen, Terrace, LiveGraph, GraphOne, STINGER and LLAMA have been screened as serious dynamic-graph systems, but their public contracts do not match exact BFS-state maintenance after every identical batch. They remain outside the primary dynamic-BFS latency table.

## Public-dataset and large-scale evidence

Current engineering evidence includes exact 100M-edge-class triangle validation, 10M/100M storage measurements, repeated steady-state maintenance, a 100M+-class canonicalization-policy A/B, and the repeated native two-dataset dynamic-BFS campaign.

The project does not yet establish broad publication-grade results across many graph families, many-core scaling, controlled multi-socket NUMA, same-machine VeloGraphX/RisGraph/NetworKit comparison, comprehensive hardware counters, research-scale compression calibration or dedicated NVMe evaluation.

## Benchmark interpretation

Hosted CI validates correctness, benchmark contracts, reproducibility tooling and engineering behavior on documented workloads. It is **not** a publication-grade performance environment.

Performance should be promoted only when dataset provenance, checksums, machine/toolchain details, competitor revisions, repetitions/statistics, correctness checks and retained artifacts are captured by the documented workflow.

For the detailed status matrix, see [`prompt-coverage.md`](prompt-coverage.md). For hosted-CI evidence and its boundaries, see [`ci-scale-evidence.md`](ci-scale-evidence.md).
