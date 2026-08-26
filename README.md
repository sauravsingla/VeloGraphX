# VeloGraphX

VeloGraphX is a CPU-native incremental graph analytics engine for large, continuously changing graphs.

> Guiding question: **when a massive graph changes only slightly, how little work can a modern CPU perform while still producing the correct updated answer?**

## Implemented

- C++20/CMake core with Linux and macOS CI
- Static CSR foundation and edge-list loading
- Static BFS, connected components, PageRank, triangle count, common neighbors and Jaccard
- Versioned dynamic graph with batch insert/delete deltas and threshold compaction
- Incremental triangle counting, insertion-optimized connected components, BFS, unweighted SSSP and localized PageRank repair
- k-core maintenance baseline with correct recomputation fallback
- Adaptive scalar/galloping intersection plus runtime ISA detection hooks for AVX2, AVX-512 and NEON
- Sparse/dense frontier and push/pull selection policies
- Incremental-vs-recompute cost estimator with explain output
- Portable multicore thread pool and graph partition helper
- Aligned allocation, reversible adjacency delta compression baseline, memory-budget primitive and NUMA API surface
- Native binary graph I/O
- Optional pybind11 Python module
- Dynamic benchmark harness and formal ablation plan
- Research landscape, novelty ledger, limitations and paper outline

## Explicit limitations

Architecture-specific SIMD paths currently preserve correctness by falling back to portable intersection until calibrated intrinsic implementations are added. Native NUMA placement and affinity require platform-specific implementation and multi-socket validation. Weighted mutable edges, NumPy/SciPy/Arrow zero-copy adapters and NVMe/io_uring out-of-core execution remain future research/engineering milestones. VeloGraphX does not make performance or novelty claims without measurements. See `docs/limitations.md`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Python bindings require pybind11 and can be enabled with `-DVELOGRAPHX_BUILD_PYTHON=ON`.

## Research discipline

Avoid unnecessary work first, improve locality second, use multicore parallelism third, and optimize instructions fourth. Every incremental result must be checked against full recomputation; every optimization needs an ablation; every novelty claim needs documented prior art.

## License

Apache-2.0.
