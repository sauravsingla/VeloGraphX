# VeloGraphX

VeloGraphX is a CPU-native incremental graph analytics engine for large, continuously changing graphs.

> Guiding question: **when a massive graph changes only slightly, how little work can a modern CPU perform while still producing the correct updated answer?**

## Implemented today

- Modern C++20, CMake and portable scalar fallback
- Static CSR graph foundation and edge-list loading
- Static BFS, connected components, PageRank, triangle counting, common neighbors and Jaccard
- Dynamic graph with versioned batches, insertion/deletion deltas and threshold compaction
- Incremental triangle counting
- Insertion-optimized connected components with safe deletion rebuild
- Incremental BFS and unweighted SSSP with deletion rebuild
- Localized PageRank repair and k-core recomputation baseline
- Adaptive sorted/galloping neighbor intersection policy with ISA-aware dispatch hooks
- Sparse/dense frontier representation
- Transparent incremental-vs-full execution estimate and explain output
- Portable thread pool, aligned allocator abstraction, memory-budget primitive and NUMA API surface
- Native binary graph I/O
- Optional pybind11 Python bindings
- Static and dynamic benchmark smoke harnesses with JSON-style output
- CI on Linux/macOS and ASan/UBSan
- Research landscape, novelty ledger, ablation plan and paper outline

## Explicitly experimental / not yet claimed complete

AVX2/AVX-512/NEON intrinsic kernels are API-defined but currently fall back to portable code; real NUMA placement requires platform-specific implementation and multi-socket validation; weighted dynamic SSSP, compressed adjacency, NumPy/SciPy/Arrow adapters and NVMe/io_uring out-of-core execution remain future work. See `docs/limitations.md`. VeloGraphX does not make performance or novelty claims without measurements.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Enable Python bindings when pybind11 is installed:

```bash
cmake -S . -B build -DVELOGRAPHX_BUILD_PYTHON=ON
```

## Repository map

- `include/velographx/storage/` static/dynamic graph storage and memory budget
- `include/velographx/incremental/` incremental algorithm state
- `include/velographx/kernels/` graph primitives and dispatch
- `include/velographx/runtime/` planner, frontier, multicore and NUMA surfaces
- `include/velographx/memory/` allocation primitives
- `include/velographx/io/` graph I/O
- `include/velographx/metrics/` observability structures
- `benchmarks/` reproducible benchmark entry points
- `docs/research/` prior work, hypotheses, novelty and ablations
- `python/` optional bindings
- `paper/` research-paper working material

## Research discipline

Every incremental result must be checked against full recomputation. Every optimization needs an ablation. Every novelty claim needs documented prior-art analysis. Negative results are retained. Smoke benchmark numbers are not research results.

## License

Apache-2.0.
