# Prompt coverage matrix

This document maps the repository against the original VeloGraphX implementation prompt. A feature is marked complete only when implementation and correctness/build coverage exist; performance claims additionally require measurements.

## Complete / implemented

- Milestone 0 research landscape, novelty ledger, architecture and benchmark methodology.
- Milestone 1 static C++20 graph foundation, loaders, BFS, connected components, PageRank, triangle counting, tests and benchmark harness.
- Hybrid dynamic storage baseline with CSR-like base adjacency, per-vertex insertion/deletion deltas, graph versions, update batches and threshold compaction.
- Incremental triangle counting, BFS/unweighted SSSP, localized connected-components deletion repair, localized dynamic k-core repair and residual-driven localized PageRank repair.
- Weighted mutable graph storage and weighted incremental SSSP, including safe full-recompute fallback for destructive weight changes/deletions.
- Affected-work execution estimates with observed affected-edge fraction, historical speedup, repair success, confidence, estimated work fraction and explainable incremental-vs-full selection.
- Adaptive neighbor intersection with scalar, galloping, bitmap and architecture-specific AVX2/AVX-512/NEON intrinsic paths plus runtime ISA detection and differential tests.
- Sparse/dense frontier policy, push/pull policy, partition helper, graph-specific work stealing, adaptive grain size, degree/frontier-aware scheduling and concurrency stress coverage.
- Linux NUMA topology discovery, CPU affinity, mmap-backed memory regions, first-touch support, mbind bind/interleave policy, graph partition placement, NUMA-local queue routing and local-first stealing observability, with portable fallback.
- Compression codecs: reversible delta coding, variable-byte delta coding, blocked variable-byte coding and SIMD-friendly fixed-width 1/2/4-byte delta blocks with metadata and round-trip/error coverage.
- Architecture-specific fixed-width decode paths using AVX2 and NEON with scalar fallback and runtime backend selection.
- Adaptive compression codec recommendation based on exact encoded byte counts, available vector decode backend and configurable fixed-width size overhead, with correctness coverage.
- Native binary graph format and Python bindings with NumPy, SciPy CSR and Apache Arrow ingestion; incremental BFS, connected components, k-core, PageRank and weighted dynamic SSSP bindings; lifetime-safe keep-alive ownership; and Python CI checks.
- Timestamped graph history, snapshot/change-window APIs and sliding-window primitives.
- Out-of-core partition-file backend with mmap/fallback reads, async loading, bounded partition cache, Linux kernel readahead hints and an opt-in Linux liburing/io_uring prefetch path with portable fallback.
- Loader hardening, malformed-input regression coverage, randomized dynamic mutation campaigns and concurrent work-stealing stress tests.
- Checksum-verified dataset preparation tooling with local fixture coverage and explicit SHA-256 mismatch validation.
- Competitor benchmark adapters for builtin reference, NetworkX, igraph, NetworKit and rustworkx, plus a normalized external/native command contract.
- Concrete native adapter shims and reproducibility recipes for locally installed SuiteSparse:GraphBLAS/LAGraph and GAP BFS runners, with deterministic contract fixtures and explicit failure when native binaries are unavailable.
- Reproducible machine-readable experiment orchestration and campaign definitions for update-fraction crossover, thread scaling, NUMA placement, hardware counters and ablations.
- Codec throughput/compression-ratio benchmark tooling across dense, medium and sparse graph-neighbor families, including scalar versus vectorized fixed-width decode measurements.
- Codec-policy calibration tooling that derives machine-readable thresholds from supplied benchmark CSV while explicitly separating synthetic/self-test data from research claims.
- CI on Ubuntu/macOS, ASan/UBSan, observability structures, ablation suite, paper scaffold and benchmark tooling.

## Partial — implementation exists but original prompt requires deeper capability or validation

- NUMA execution is implemented at policy/runtime level, but true multi-socket locality, bandwidth and remote-traffic experiments remain environment-dependent.
- Adaptive compression selection and calibration tooling are implemented, but the production fixed-width crossover/overhead threshold still requires calibration from dedicated public-dataset codec campaigns.
- Python interoperability covers the major dynamic/incremental algorithms implemented by the engine; additional convenience APIs and future algorithms can still be exposed as they are added.
- Out-of-core storage supports mmap, async loading, bounded caching, readahead and optional io_uring prefetch; research-scale NVMe evaluation remains environment-dependent.
- Adaptive execution planning is implemented, but crossover thresholds still require large benchmark campaigns on public datasets.
- Native LAGraph/GraphBLAS and GAP wrapper contracts are implemented, but full competitor execution still requires those projects to be built and version-pinned on the target benchmark machine.

## Not measured / environment-dependent

- True multi-socket NUMA locality/remote-traffic measurements.
- Large-scale 1/2/4/8/16/32+ thread scaling on dedicated hardware.
- 100M+ edge research prototype measurements.
- Full update-fraction crossover campaign at 0.0001%, 0.001%, 0.01%, 0.1%, 1%, 5% and 10% on public research-scale datasets.
- Full competitor campaign against NetworkX, igraph, NetworKit, rustworkx, SuiteSparse:GraphBLAS/LAGraph and GAP on equivalent datasets/hardware.
- Publication-grade ablation and hardware-counter measurements on dedicated hardware.
- Research-scale codec throughput/compression-ratio campaign across public graph families.
- Research-scale NVMe/io_uring throughput and overlap measurements.

## Future / genuinely remaining engineering milestones

- Execute representative public-dataset codec campaigns and feed measured CSV into the calibration tool to select production codec thresholds.
- Build/version-pin SuiteSparse:GraphBLAS/LAGraph and GAP on the target benchmark environment and execute the normalized competitor campaign.
- Execute the full update-size crossover, scaling, NUMA, perf-counter and ablation campaigns on dedicated hardware.
- Research-scale evaluation at 100M+ edges and publication-quality machine-readable result artifacts.

No unmeasured item is represented as completed, and no benchmark or novelty result is invented.
