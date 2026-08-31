# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact analytics on changing graphs.** It combines compact mutable graph storage, localized exact repair, adaptive incremental-vs-full recomputation, and reproducible benchmarking.

## Highlights

| Result | Validated evidence |
| --- | --- |
| Dynamic BFS vs GAP | **2.84x–3.79x lower median kernel latency** across 1/2/4 threads on the normalized hosted-CI workload, with identical graph/source, 5 repetitions, CPU affinity, and correctness gates |
| Dynamic BFS vs NetworKit 11.2.1 | **23.7% lower latency** on `web-Google` with identical updates, 1 thread, 5 paired repetitions, and independent full-BFS verification |
| Exact dynamic triangles, `com-LiveJournal` | **3,228.66x**, **382.85x**, and **33.99x** faster than full recomputation at 0.01%, 0.1%, and 1% update batches |
| Exact triangles vs GoldenCounter | **3.48x–40.95x lower latency**, with **15/15 exact matches** on the evaluated workload |
| Large-graph exact validation | `com-Orkut`: **117,185,083 edges** and **627,584,181 initial triangles** |
| Adaptive BFS | **100% exactness**, **3.15% mean oracle-relative regret**, **7.49 µs** mean selector cost |

These are workload-specific measurements, not universal performance claims. Publication-grade 8/16/32+ core scaling, genuine NUMA evaluation, and largest-practical-memory measurements require dedicated hardware.

## What VeloGraphX does

When a graph changes, VeloGraphX decides whether to repair the existing exact result locally or recompute it from scratch.

```text
update stream
    ↓
mutable graph storage
    ↓
work / cost estimation
    ↓
exact localized repair  ↔  full recomputation
    ↓
CPU execution
    ↓
exact maintained result
```

### Algorithms

- BFS / unweighted SSSP
- weighted SSSP
- connected components
- exact triangle counting
- k-core
- PageRank repair

The graph algorithms themselves are established techniques. The research contribution is the **systems integration of mutable storage, exact dynamic repair, adaptive execution selection, CPU execution, and reproducible evaluation**.

## System design

| Area | Implementation |
| --- | --- |
| Dynamic storage | Segmented CSR, packed deltas, sparse row patches, reverse adjacency, validated consolidation |
| Algorithm/storage separation | C++20 graph-access contract; `BasicIncremental*<Graph>` implementations; `DynamicGraph` and read-optimised `CsrGraph` backends |
| Adaptive execution | Update-density preflight, affected-work signals, graph-scale conditioning, online cost estimates, uncertainty-aware selection |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20, pybind11, NumPy, SciPy CSR, Apache Arrow |
| Reproducibility | Checksum-pinned datasets, pinned baselines, exactness gates, environment capture, retained artifacts |

### Storage-independent algorithm layer

Core incremental implementations are templated on graph representation rather than hard-wired to `DynamicGraph`. The default public class names remain compatibility aliases for the dynamic backend, while the reusable `BasicIncrementalBFS<Graph>`, `BasicIncrementalSSSP<Graph>`, `BasicIncrementalComponents<Graph>`, `BasicIncrementalKCore<Graph>`, `BasicIncrementalPageRank<Graph>`, and `BasicIncrementalTriangleCount<Graph>` implementations can be instantiated over compatible representations.

Hot traversal paths use callback/span-style adjacency access instead of materialising a `std::vector` per vertex visit. `CsrGraph` carries both forward and reverse CSR so the same read algorithm can be executed against the mutable VeloGraphX layout and a read-optimised layout. `velographx_backend_bfs_benchmark` runs the exact same BFS implementation on both backends and gates the timing output on identical distance vectors.

See [Storage-independent graph algorithm contract](docs/graph-abstraction.md).

## Selected validated results

### Dynamic BFS vs GAP

A hosted-CI benchmark runs VeloGraphX and GAP on the **same generated graph, same BFS source, same runner, same CPU affinity, and five repetitions per thread count**. Every measured run passes correctness verification.

| Threads | VeloGraphX median | GAP median | Lower latency |
| ---: | ---: | ---: | ---: |
| 1 | **0.058 ms** | 0.180 ms | **3.10x** |
| 2 | **0.058 ms** | 0.220 ms | **3.79x** |
| 4 | **0.074 ms** | 0.210 ms | **2.84x** |

The workload is intentionally small (4,096 vertices, 16,384 edges), so this result demonstrates a correctness-gated normalized comparison rather than large-scale multicore scalability.

Run `33360607334`; retained artifact `9746891819`.

### Dynamic BFS vs NetworKit

On `web-Google`, a native C++ comparison with NetworKit 11.2.1 uses one thread, identical update streams, five paired repetitions, and independent full-BFS verification after every batch.

| VeloGraphX | NetworKit | Result |
| ---: | ---: | ---: |
| **31.512 ms** | 41.293 ms | **23.7% lower latency** |

Run `33301190847`; artifact `9729078197`.

### Exact dynamic triangles

On `com-LiveJournal` with approximately **34.7 million base edges**:

| Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: |
| 0.01% | 4.066 ms | 13.126 s | **3,228.66x** |
| 0.1% | 34.709 ms | 13.288 s | **382.85x** |
| 1% | 415.377 ms | 14.119 s | **33.99x** |

Large-graph exact validation also completed on `com-Orkut` with **117,185,083 edges** and **627,584,181 initial triangles**.

Against the pinned exact `GoldenCounter` component from the public SIGMOD 2021 source, **15/15 results matched exactly**, with **3.48x–40.95x lower latency** on the evaluated workload.

### Adaptive BFS

The development suite uses checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google` workloads and includes selector decision cost in adaptive timing.

| Metric | Result |
| --- | ---: |
| Exactness | **100%** |
| Mean oracle-relative regret | **3.15%** |
| p95 batch regret | **19.78%** |
| Worst-regime regret | **18.25%** |
| Mean selector cost | **7.49 µs** |

A controlled crossover campaign covers **3 graph families × 3 roots × 3 update regimes × 5 repetitions**, with independent full-BFS verification.

### Public benchmark contract

VeloGraphX also maintains reproducible scale-free, road-network, and Graph500-style Kronecker/R-MAT benchmark paths. Kernel time is reported separately from preprocessing-inclusive end-to-end time.

A retained hosted run on SNAP `roadNet-CA` verified **1,965,206 source vertices**, **2,766,607 undirected roads**, and produced:

| Operation | Kernel | End-to-end |
| --- | ---: | ---: |
| BFS | **69.801 ms** | 2.035 s |
| Connected components | **69.059 ms** | 2.034 s |
| Triangle counting | **62.417 ms** | 2.028 s |
| PageRank | **2.671 s** | 4.636 s |

Run `33355564089`; artifact `9745019076`.

## Correctness and reproducibility

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, graph mutation/storage consistency, incremental-vs-full differential testing, SIMD/scalar agreement, Python interoperability, dataset provenance, benchmark contracts, and independent reference checks.

Experiments use checksum-pinned datasets, immutable baseline revisions, explicit thread settings, repeated measurements, exactness gates, environment capture, and retained GitHub Actions artifacts.

Key methodology documents:

- [Storage-independent graph algorithm contract](docs/graph-abstraction.md)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [External dynamic baselines](docs/external-dynamic-baselines.md)
- [External baseline timing contract](docs/external-baseline-timing-contract.md)
- [Ablation study](docs/ablation-study.md)
- [Related-work positioning](docs/related-work-positioning.md)
- [Current limitations](docs/limitations.md)

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Enable Python bindings with `-DVELOGRAPHX_BUILD_PYTHON=ON`.

## License

Licensed under the [Apache License 2.0](LICENSE).
