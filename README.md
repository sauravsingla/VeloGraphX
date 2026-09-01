# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact analytics on changing graphs.** It combines mutable graph storage, localized exact repair, and workload-aware incremental-vs-full recomputation.

## Research focus

VeloGraphX does not claim novelty for BFS, SSSP, connected components, triangle counting, k-core, PageRank, or incremental graph processing individually. Its research focus is the **system-level coupling of mutable storage, exact localized repair, affected-work/cost signals, and adaptive repair-vs-recompute selection**.

The runtime can choose recomputation before excessive repair work is incurred, avoiding a repair-then-recompute penalty when an update batch crosses the incremental crossover region.

```text
updates → mutable graph → work/cost estimation
        → localized exact repair  ↔  full recomputation
        → exact maintained result
```

## Validated results

Results below are retained GitHub Actions measurements from 31 August 2026. They are workload-specific engineering evidence, not publication-grade hardware claims.

### GAP + LAGraph comparison

Five repetitions per configuration, same hosted Linux runner, same source vertex, correctness checks enabled, `OMP_PLACES=cores`, `OMP_PROC_BIND=spread`.

| Algorithm | Threads | VeloGraphX | GAP | LAGraph |
| --- | ---: | ---: | ---: | ---: |
| BFS | 1 | **0.425 ms** | 0.790 ms | 4.000 ms |
| BFS | 2 | **0.420 ms** | 0.670 ms | 4.000 ms |
| BFS | 4 | **0.406 ms** | 0.830 ms | 4.800 ms |
| SSSP | 1 | 8.982 ms | **1.060 ms** | 23.500 ms |
| SSSP | 2 | 8.587 ms | **1.190 ms** | 23.600 ms |
| SSSP | 4 | 9.003 ms | **1.290 ms** | 27.100 ms |

VeloGraphX was fastest for BFS on this workload; GAP was substantially faster for SSSP; VeloGraphX was faster than LAGraph for both. All measured configurations passed correctness checks.

Dynamic BFS also showed the intended crossover: incremental repair beat VeloGraphX full recomputation at **0.1% and 1%** updates, while full recomputation became faster at **5%**. See [hosted native methodology](docs/hosted-native-competitors.md). Run `33418520303`, artifact `9768499895`.

### Adaptive BFS policy

Checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google`; 3 roots × 3 update regimes × 5 repetitions.

| Metric | Result |
| --- | ---: |
| Exactness | **100%** |
| Adaptive regime wins | **19 / 27** |
| Mean relative to regime-best | **1.0274x** |
| Mean overhead from regime-best | **~2.74%** |

Run `33410705480`, artifact `9767029881`.

### Dynamic BFS vs NetworKit 11.2.1

On `web-Google`, one thread, identical update streams, five paired repetitions, independent full-BFS verification:

| VeloGraphX | NetworKit |
| ---: | ---: |
| **36.328 ms** | 45.740 ms |

Run `33301190847`, artifact `9766977170`.

## Capabilities

- Exact BFS / unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank
- Segmented CSR, packed deltas, sparse row patches, reverse adjacency, and validated consolidation
- Storage-independent `BasicIncremental*<Graph>` algorithm implementations
- Adaptive exact execution using update density, affected-work signals, graph scale, online cost estimates, and uncertainty-aware selection
- SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, and NUMA-aware policies
- C++20, pybind11, NumPy, SciPy CSR, and Apache Arrow interoperability

## Correctness and reproducibility

CI covers Ubuntu/macOS builds, Linux ASan/UBSan, mutation/storage consistency, incremental-vs-full differential testing, SIMD/scalar agreement, Python interoperability, dataset provenance, benchmark contracts, and independent reference checks.

Experiments use pinned datasets or baseline revisions, repeated measurements, exactness gates, environment capture, and retained artifacts. Hosted CI is treated as engineering evidence; dedicated controlled hardware is still required for publication-grade scalability, NUMA, and hardware-counter claims.

## Documentation

- [Benchmark methodology](docs/benchmark-methodology.md)
- [Hosted GAP/LAGraph methodology](docs/hosted-native-competitors.md)
- [Graph abstraction](docs/graph-abstraction.md)
- [External dynamic baselines](docs/external-dynamic-baselines.md)
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

Python bindings:

```bash
cmake -S . -B build -DVELOGRAPHX_BUILD_PYTHON=ON
```

## License

Licensed under the [Apache License 2.0](LICENSE).
