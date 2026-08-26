# VeloGraphX

VeloGraphX is a CPU-native incremental graph analytics engine for large, continuously changing graphs.

> Guiding question: **when a massive graph changes only slightly, how little work can a modern CPU perform while still producing the correct updated answer?**

VeloGraphX is intentionally CPU-first. The project focuses on reducing unnecessary recomputation, improving memory locality, exploiting multicore execution, and using architecture-aware kernels before considering accelerator-specific execution.

## Implemented

- Modern C++20 core with CMake, Linux/macOS CI, ASan/UBSan coverage, examples, tests, and benchmark smoke runs.
- Static CSR graph foundation with edge-list loading and native binary graph I/O.
- Static BFS, connected components, PageRank, triangle counting, common neighbors, Jaccard similarity, k-core baseline, and SSSP support.
- Versioned dynamic graph storage with batched insert/delete deltas, graph versions, threshold-based compaction, and weighted dynamic graph support.
- Incremental triangle counting, insertion-optimized connected components, BFS, unweighted SSSP, weighted incremental SSSP, and localized PageRank repair with safe recomputation fallbacks where destructive updates can invalidate maintained state.
- Incremental-versus-full-recompute execution planner with affected-work estimation and explain output baseline.
- Adaptive neighbor intersection with scalar, galloping, bitmap, AVX2, AVX-512, and ARM NEON implementations plus runtime ISA detection and scalar-reference differential tests.
- Sparse/dense frontier policies and push/pull selection helpers for traversal-oriented algorithms.
- Portable multicore thread pool plus graph-oriented work-stealing runtime with per-worker queues, locality hints, adaptive grain sizing, parallel-for support, and runtime statistics.
- Linux NUMA topology discovery and CPU-list parsing with portable fallback behavior.
- Aligned allocation primitives and memory-budget abstraction.
- Adjacency compression support including delta encoding, variable-byte encoding, and blocked compressed adjacency with correctness coverage.
- Timestamped temporal graph history with stable version snapshots, time-based snapshots, changes-between-version/time queries, and sliding-window reconstruction support.
- Optional pybind11 Python module with NumPy edge-array ingestion and SciPy CSR ingestion without Python edge-tuple materialization.
- Dynamic/static/intersection benchmark harnesses, research landscape, novelty ledger, ablation plan, benchmark methodology, limitations, and paper scaffold.

## What remains experimental or incomplete

VeloGraphX does **not** claim that every planned research milestone is complete. In particular, native NUMA-local allocation, first-touch placement, CPU affinity, node-local graph scheduling, and multi-socket locality measurements still require deeper platform-specific implementation and dedicated hardware validation. The current multicore runtime includes work stealing and adaptive grain sizing, but full degree/frontier-aware scheduling and large-scale scaling measurements remain research work.

Incremental connected-components deletions, destructive dynamic k-core updates, and PageRank repair still need stronger localized repair/propagation strategies before they can be described as fully optimized dynamic algorithms. Apache Arrow interoperability, broader Python exposure of dynamic/incremental state, ownership/lifetime documentation, and Python-specific CI coverage also remain incomplete.

The out-of-core/NVMe milestone is not yet complete: a real partitioned backend with mmap, optional Linux io_uring prefetch, bounded resident cache/eviction, and memory-budget-driven residency control remains future engineering work. Large public-dataset evaluation, 100M+ edge experiments, full update-fraction crossover studies, competitor comparisons, hardware-counter measurements, and the complete ablation matrix are also **not measured yet** unless explicitly documented otherwise.

VeloGraphX does not make unsupported performance or novelty claims. See `docs/prompt-coverage.md`, `docs/limitations.md`, and `docs/benchmark-methodology.md` for the current implementation and measurement status.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Python bindings require pybind11 and can be enabled with:

```bash
cmake -S . -B build -DVELOGRAPHX_BUILD_PYTHON=ON
cmake --build build -j
```

## Research discipline

VeloGraphX follows four priorities: **avoid unnecessary graph work first, improve locality second, scale across CPU cores/NUMA domains third, and optimize instructions fourth**. Incremental results should be checked against full recomputation, optimization claims should be supported by ablations, and novelty claims should be backed by documented prior work and reproducible experiments.

## Project status

The repository is under active development. Implemented features are distinguished from partial, unmeasured, and future work in `docs/prompt-coverage.md`. Benchmark numbers should only be treated as project claims when they are generated by the documented reproducible benchmark workflow.

## License

Apache-2.0.
