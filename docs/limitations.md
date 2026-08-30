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

The repository now has matched hosted-CI dynamic-BFS evidence against two serious external systems: the native SIGMOD 2021 RisGraph implementation and NetworKit 11.2.1 `DynBFS`. Both comparisons use checksum-pinned `web-Google`, the same source-order sliding-window semantics, deterministic root selection, answer-ready timing, and exact correctness gates. See [`external-dynamic-baselines.md`](external-dynamic-baselines.md).

These results still have important limits. The RisGraph and NetworKit campaigns were separate hosted-runner executions and therefore cannot be combined into a three-system ranking. The NetworKit baseline is invoked through Python bindings, so its graph-mutation timing includes Python API overhead while its dynamic BFS maintenance is native; a native C++ harness is required before treating that pairwise ratio as publication-grade language-neutral evidence.

Teseo/GFE, Aspen, Terrace, LiveGraph, GraphOne, STINGER, and LLAMA have been screened as serious dynamic-graph systems. Their public evaluation contracts emphasize structural updates and/or graph kernels over updated snapshots rather than exact BFS-state maintenance after every identical batch. They should therefore be evaluated in separately labelled structural-update or snapshot-query experiments rather than inserted into the same incremental-BFS speedup table.

The repository also contains normalized competitor adapters and reproducibility contracts for NetworkX, igraph, NetworKit, rustworkx, LAGraph/GraphBLAS, and GAP. Publication-grade comparative claims still require repeated same-hardware runs, native timing envelopes where available, immutable pins, controlled thread placement, multiple graph families and seeds, and retained statistical artifacts.

## Public-dataset and large-scale evidence

Public-dataset engineering evidence now includes exact 100M-edge-class triangle validation, 10M/100M storage measurements, repeated steady-state maintenance, and a 100M+-class canonicalization-policy A/B. These results establish engineering and scale evidence on the stated workloads, but the overall evaluation remains incomplete.

The project does not yet claim results for:

- a broad multi-dataset incremental crossover campaign across substantially different graph families;
- publication-grade repeated 100M+-edge comparisons across multiple algorithms, datasets, seeds, and external systems;
- publication-grade 1/2/4/8/16/32+ thread scaling;
- controlled multi-socket NUMA locality and remote-traffic studies;
- a dedicated-hardware native same-run comparison against the full serious dynamic-system baseline set;
- comprehensive hardware-counter and ablation campaigns;
- research-scale compression calibration; or
- dedicated NVMe / `io_uring` throughput evaluation.

## Benchmark interpretation

Hosted CI validates correctness, contracts, reproducibility tooling, and engineering behavior on the documented workloads. It is **not** a publication-grade performance environment.

Performance results should only be promoted to project-level research claims when the corresponding dataset provenance, checksums, machine configuration, compiler/toolchain information, competitor versions, repetitions/statistics, correctness checks, and result artifacts are captured by the repository's documented workflow.

For the most detailed status matrix, see [`prompt-coverage.md`](prompt-coverage.md). For measured hosted-CI evidence and its boundaries, see [`ci-scale-evidence.md`](ci-scale-evidence.md).
