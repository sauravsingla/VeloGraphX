# Prompt coverage matrix

This document maps the repository against the original implementation prompt.

## Implemented now

- Milestone 0 research landscape, novelty ledger, architecture and benchmark methodology.
- Milestone 1 static C++20 graph foundation, loaders, BFS, connected components, PageRank, triangle counting, tests and benchmark harness.
- Hybrid dynamic storage baseline with CSR-like base adjacency, per-vertex insertion/deletion deltas, graph versions, update batches and threshold compaction.
- Incremental triangle counting, insertion-oriented connected components, BFS, unweighted SSSP and localized PageRank repair, plus k-core correctness baseline.
- Affected-work execution estimate, incremental-vs-full plan selection and explain output.
- Adaptive neighbor intersection policy, scalar and galloping kernels, CPU feature detection, and stable AVX2/AVX-512/NEON dispatch surfaces.
- Sparse/dense frontier policy, push/pull policy, partition helper, portable multicore thread pool.
- Aligned allocation, reversible adjacency delta compression baseline, memory budget abstraction and NUMA API surface.
- Native binary graph format, optional pybind11 bindings, CI/sanitizers, observability structures, ablation plan and paper scaffold.

## Partially implemented / correctness baseline

- SIMD: runtime ISA detection and dispatch exist, but architecture-specific paths intentionally fall back to scalar until intrinsic kernels are benchmark-calibrated.
- NUMA: API/design exists, but native libnuma placement, affinity and multi-socket experiments are not claimed complete.
- Compression: delta encoding/decoding exists, but variable-byte and blocked SIMD codecs remain experimental.
- Dynamic connected-components deletion and k-core updates use full rebuild for correctness.
- PageRank repair is localized but not yet residual-theory optimized for all graph/update cases.

## Future hardware/data integration milestones

- Weighted mutable-edge schema and weighted SSSP.
- NumPy/SciPy/Arrow zero-copy adapters with ownership-safe semantics.
- NVMe partition backend, mmap/io_uring asynchronous prefetch and full memory-budget residency controller.
- Native multi-socket NUMA allocator/affinity policies.
- Calibrated intrinsic AVX2/AVX-512/NEON implementations and architecture-specific crossover tables.
- Large public dataset campaign, all requested update fractions, competitor baselines and full ablation matrix.

No unmeasured item is represented as completed, and no benchmark or novelty result is invented.
