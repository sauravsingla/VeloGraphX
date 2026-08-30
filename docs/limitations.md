# Current limitations

VeloGraphX implements a broad set of dynamic, incremental, CPU-optimization, interoperability, and reproducibility capabilities, but several areas still require broader evaluation or remain environment-dependent. This document records those limits so implemented engineering capability is not confused with publication-grade evidence.

## SIMD and architecture-specific kernels

Architecture-specific neighbor-intersection paths are implemented for AVX2, AVX-512, and ARM NEON, with runtime ISA detection and scalar reference/fallback behavior. Differential testing is used to protect correctness.

The remaining limitation is **evaluation coverage**, not absence of implementation: intrinsic paths have not yet been comprehensively calibrated across a wide range of dedicated x86 and ARM systems, graph degree distributions, compiler versions, and microarchitectures. Hosted CI should therefore be treated as correctness and engineering evidence rather than proof of universal SIMD speedup.

## Incremental algorithm coverage

Localized or incremental update paths are implemented for BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core, and PageRank-related workflows. The engine can explicitly fall back to full recomputation when an update is destructive, a localized repair cannot be proven safe, or adaptive policy indicates that repair is unlikely to be worthwhile.

This means the project does **not** claim that every update to every supported algorithm is always repaired incrementally. Characterizing the crossover point between localized repair and full recomputation across graph families and update patterns remains an active benchmarking task.

## Weighted dynamic graphs

Weighted graph updates and weighted SSSP support are implemented. Destructive weighted changes may require conservative full recomputation when a safe localized repair path is not available.

The remaining work is broader evaluation of weighted incremental behavior on public datasets, including update-fraction crossover studies and comparison with relevant CPU graph systems.

## NUMA

Linux NUMA support includes topology discovery, CPU affinity, first-touch behavior, `mbind`-based placement where available, graph partition placement, local queues, and local-first stealing/observability. Portable fallback behavior is retained for systems where Linux NUMA facilities are unavailable.

The major limitation is that **true multi-socket NUMA behavior has not yet been established at publication grade**. Hosted CI cannot substitute for controlled experiments measuring remote-memory traffic, locality, bandwidth, cross-socket stealing, and scaling on dedicated multi-socket hardware.

## Multicore scaling

The repository includes multicore execution infrastructure such as sparse/dense frontier policies, push/pull selection, work stealing, adaptive grain sizing, partitioning, and degree/frontier-aware scheduling.

Current hosted-CI evidence is intentionally small scale. The project does not yet claim stable 1/2/4/8/16/32+ core scaling or superiority over other graph engines on dedicated many-core systems. Those claims require controlled CPU pinning, repeated measurements, memory-bandwidth analysis, and identical-hardware competitor runs.

## Out-of-core execution

Out-of-core infrastructure is implemented through partition files, mmap/fallback reads, asynchronous loading, a bounded cache, readahead, and optional Linux `io_uring` prefetch support.

This should not yet be interpreted as evidence of research-scale NVMe performance. Large datasets, controlled storage devices, cache-state management, queue-depth studies, I/O amplification measurements, and `io_uring` versus fallback comparisons remain to be evaluated on dedicated hardware.

## Compression

VeloGraphX implements delta, variable-byte, blocked variable-byte, and SIMD-friendly fixed-width adjacency coding together with codec-selection/calibration infrastructure.

Codec thresholds are not yet claimed to be universally optimal. Production-quality selection thresholds require larger public-dataset campaigns covering different graph structures, compression ratios, decode throughput, cache behavior, and CPU architectures.

## Python interoperability

Python bindings are optional and require `pybind11` at configure/build time. NumPy, SciPy CSR, and Apache Arrow ingestion paths are supported when their Python dependencies are available.

The bindings are suitable for experimentation and interoperability, but the Python API should not yet be treated as a frozen compatibility surface. Additional convenience APIs, packaging, binary-wheel distribution, and long-term API stability remain future work.

## Competitor evaluation

The repository has matched dynamic-BFS evidence against two serious external systems: the native SIGMOD 2021 RisGraph implementation and NetworKit 11.2.1 `DynBFS`. The NetworKit comparison is now **native C++**, repeated five times per dataset, and executed sequentially with VeloGraphX on the same hosted runner. It covers checksum-pinned `web-Google` and `ca-GrQc`, exact correctness after every batch, cross-system layer-histogram equality, captured environment/pins, and explicit nontrivial-reachability gates. See [`external-dynamic-baselines.md`](external-dynamic-baselines.md).

The native NetworKit campaign is substantially stronger than the earlier Python-binding comparison and supersedes it as the primary NetworKit performance evidence. On accepted run `33291065285`, NetworKit was approximately **1.31x faster on web-Google** and **4.86x faster on ca-GrQc**. These results are retained precisely because the benchmark framework must report unfavorable external outcomes as readily as favorable ones.

Important limits remain:

- the native NetworKit campaign is still hosted CI rather than dedicated hardware;
- it uses one OpenMP thread and only two public graph families;
- one deterministic root/workload is used per dataset;
- the existing RisGraph result is from a separate hosted runner, so RisGraph and NetworKit cannot yet be placed into one absolute three-system ranking; and
- publication-grade evidence still requires dedicated same-machine native runs, controlled CPU placement/frequency, more repetitions, multiple roots/seeds/update regimes, and multicore scaling.

Teseo/GFE, Aspen, Terrace, LiveGraph, GraphOne, STINGER, and LLAMA have been screened as serious dynamic-graph systems. Their public evaluation contracts emphasize structural updates and/or graph kernels over updated snapshots rather than exact BFS-state maintenance after every identical batch. They should therefore be evaluated in separately labelled structural-update or snapshot-query experiments rather than inserted into the same incremental-BFS speedup table.

The repository also contains normalized competitor adapters and reproducibility contracts for NetworkX, igraph, NetworKit, rustworkx, LAGraph/GraphBLAS, and GAP. Broad comparative claims still require semantic equivalence, native timing envelopes where available, immutable pins, controlled hardware, and retained statistical artifacts.

## Public-dataset and large-scale evidence

Public-dataset engineering evidence includes exact 100M-edge-class triangle validation, 10M/100M storage measurements, repeated steady-state maintenance, a 100M+-class canonicalization-policy A/B, and a repeated native two-dataset external dynamic-BFS campaign. These establish engineering and scale evidence on the stated workloads, but the overall evaluation remains incomplete.

The project does not yet claim results for:

- a broad incremental campaign across many substantially different graph families, roots, seeds and update-locality patterns;
- publication-grade repeated 100M+-edge comparisons across multiple algorithms and external systems;
- publication-grade 1/2/4/8/16/32+ thread scaling;
- controlled multi-socket NUMA locality and remote-traffic studies;
- a dedicated-hardware native same-run comparison containing VeloGraphX, RisGraph and NetworKit together;
- comprehensive hardware-counter and ablation campaigns;
- research-scale compression calibration; or
- dedicated NVMe / `io_uring` throughput evaluation.

## Benchmark interpretation

Hosted CI validates correctness, contracts, reproducibility tooling, and engineering behavior on the documented workloads. It is **not** a publication-grade performance environment.

Performance results should only be promoted to project-level research claims when the corresponding dataset provenance, checksums, machine configuration, compiler/toolchain information, competitor versions, repetitions/statistics, correctness checks, and result artifacts are captured by the repository's documented workflow.

For the most detailed status matrix, see [`prompt-coverage.md`](prompt-coverage.md). For measured hosted-CI evidence and its boundaries, see [`ci-scale-evidence.md`](ci-scale-evidence.md).
