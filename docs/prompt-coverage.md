# Prompt coverage matrix

This document maps the repository against the original VeloGraphX implementation prompt. A feature is marked complete only when implementation and correctness/build coverage exist; performance claims additionally require measurements.

## Complete / implemented

- Milestone 0 research landscape, novelty ledger, architecture and benchmark methodology.
- Milestone 1 static C++20 graph foundation, loaders, BFS, connected components, PageRank, triangle counting, tests and benchmark harness.
- Hybrid dynamic storage baseline with CSR-like base adjacency, per-vertex insertion/deletion deltas, graph versions, update batches and threshold compaction.
- Incremental triangle counting, BFS, unweighted SSSP, insertion-oriented connected components and localized PageRank repair, plus k-core correctness baseline.
- Weighted mutable graph storage and weighted incremental SSSP, including safe full-recompute fallback for destructive weight changes/deletions.
- Affected-work execution estimate, incremental-vs-full plan selection and explain output baseline.
- Adaptive neighbor intersection with scalar, galloping, bitmap and real architecture-specific AVX2/AVX-512/NEON intrinsic paths plus runtime ISA detection and differential tests.
- Sparse/dense frontier policy, push/pull policy, partition helper and portable multicore thread-pool baseline.
- Aligned allocation, reversible adjacency delta compression baseline and memory-budget abstraction.
- Linux NUMA topology discovery and CPU-list parsing with portable fallback and CI coverage.
- Native binary graph format, optional pybind11 bindings, NumPy edge ingestion and SciPy CSR ingestion without Python edge-tuple materialization.
- CI on Ubuntu/macOS, ASan/UBSan, observability structures, ablation plan, paper scaffold and intersection calibration benchmark.

## Partial — implementation exists but original prompt requires deeper capability or validation

- NUMA execution: topology detection exists; node-local allocation, first-touch policy, affinity, graph partition placement, local queues and true multi-socket experiments remain.
- Multicore runtime: portable thread pool/partitioning exists; graph-specific work stealing, adaptive grain size, degree/frontier-aware scheduling and scaling campaign remain.
- Incremental connected-components deletion and dynamic k-core destructive updates still require localized repair rather than correctness-oriented rebuild fallback.
- Incremental PageRank is localized but needs stronger residual/dependency propagation and broader convergence/crossover validation.
- Affected-region/planner instrumentation needs richer actual-work/history signals and expanded explain output.
- Compression: delta coding exists; variable-byte, blocked and SIMD-friendly codecs plus comparative measurements remain.
- Python interoperability: NumPy/SciPy ingestion exists; Arrow interoperability, broader dynamic/incremental bindings, lifetime documentation and Python CI tests remain.
- Temporal/version support: graph versions exist; timestamped history, snapshot/change-window APIs and sliding-window primitives remain.
- Testing: unit/differential/SIMD/sanitizer coverage exists; long randomized mutation campaigns, property-style generators, concurrency stress and parser fuzzing remain.

## Not measured / environment-dependent

- True multi-socket NUMA locality/remote-traffic measurements.
- Large-scale 1/2/4/8/16/32+ thread scaling on dedicated hardware.
- 100M+ edge research prototype measurements.
- Full update-fraction crossover campaign at 0.0001%, 0.001%, 0.01%, 0.1%, 1%, 5% and 10%.
- Full competitor campaign against NetworkX, igraph, NetworKit, rustworkx, SuiteSparse:GraphBLAS/LAGraph and GAP.
- Complete ablation matrix and hardware-counter campaign.

## Future / genuinely remaining engineering milestones

- Native NUMA allocation/affinity/local scheduling policies with auto/off/interleave modes.
- Graph-specific work-stealing runtime and adaptive scheduling.
- Localized deletion repair for connected components and stronger dynamic k-core/PageRank algorithms.
- Variable-byte/blocked/SIMD-friendly adjacency compression.
- Apache Arrow interoperability and expanded Python dynamic APIs/tests.
- Timestamped temporal graph history and snapshot/window operations.
- Real partitioned out-of-core/NVMe backend with mmap, optional Linux io_uring prefetch, bounded resident cache/eviction and memory-budget residency control.
- Loader hardening/fuzzing, randomized mutation/property tests and concurrency stress tests.
- Linux perf observability tooling and machine-readable benchmark/report pipeline.
- Reproducible public-dataset tooling, update-size campaign, competitor adapters, full ablations and research-scale evaluation.

No unmeasured item is represented as completed, and no benchmark or novelty result is invented.
