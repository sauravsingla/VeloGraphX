# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact analytics on changing graphs.** It combines mutable graph storage, exact localized repair, adaptive incremental-vs-full recomputation, storage-independent algorithm implementations, and reproducible benchmarking.

## Why VeloGraphX

Dynamic graph systems must balance two costs after updates: repairing an existing result or recomputing it from scratch. VeloGraphX provides both paths and uses workload signals to choose between them while preserving exactness.

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

The project focuses on the systems integration of mutable storage, exact dynamic algorithms, adaptive execution, CPU-oriented implementation, and reproducible evaluation.

## Current validated results

The results below are from fresh reruns performed on 31 August 2026. They are workload-specific measurements, not universal performance claims.

### Dynamic BFS vs NetworKit 11.2.1

A native C++ comparison on `web-Google` uses one thread, identical update streams, five paired repetitions, and independent full-BFS verification after every batch.

| System | Mean batch latency |
| --- | ---: |
| **VeloGraphX** | **36.328 ms** |
| NetworKit 11.2.1 | 45.740 ms |

VeloGraphX delivered approximately **20.6% lower mean batch latency** in the fresh five-run comparison, with all repetitions exact.

GitHub Actions run `33301190847`; fresh retained artifact `9766977170`.

### Adaptive BFS policy

The current multi-root validation uses checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google` datasets with **3 graph families × 3 roots × 3 update regimes × 5 repetitions**.

| Metric | Fresh result |
| --- | ---: |
| Exactness | **100%** |
| Adaptive regime wins | **19 / 27** |
| Mean relative to regime-best policy | **1.0274x** |
| Mean overhead from regime-best | **~2.74%** |

Exactness is a hard workflow invariant. Timing thresholds on shared GitHub-hosted runners are retained as benchmark evidence rather than treated as deterministic correctness gates.

Current-main run `33410705480`; retained artifact `9767029881`.

### SNAP roadNet-CA

The public benchmark contract verifies **1,965,206 source vertices** and **2,766,607 undirected roads** before measurement.

| Operation | Fresh kernel time |
| --- | ---: |
| BFS | **52.848 ms** |
| Connected components | **50.538 ms** |
| Triangle counting | **54.537 ms** |
| PageRank | **2.043 s** |

GitHub Actions run `33355564089`; fresh retained artifact `9766755354`.

## Algorithms

- BFS / unweighted SSSP
- weighted SSSP
- connected components
- exact triangle counting
- k-core
- PageRank

## Architecture

| Area | Implementation |
| --- | --- |
| Dynamic storage | Segmented CSR, packed deltas, sparse row patches, reverse adjacency, validated consolidation |
| Algorithm/storage separation | C++20 graph-access contract with reusable `BasicIncremental*<Graph>` implementations |
| Read-optimized backend | Forward and reverse `CsrGraph` representation for storage-independent execution |
| Adaptive execution | Update-density preflight, affected-work signals, graph-scale conditioning, online cost estimates, uncertainty-aware selection |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20, pybind11, NumPy, SciPy CSR, Apache Arrow |
| Reproducibility | Checksum-pinned datasets, immutable baseline revisions, exactness gates, environment capture, retained artifacts |

### Storage-independent algorithm layer

Core incremental implementations are templated on graph representation rather than hard-wired to `DynamicGraph`. Compatibility aliases preserve the existing public class names, while reusable implementations such as `BasicIncrementalBFS<Graph>`, `BasicIncrementalSSSP<Graph>`, `BasicIncrementalComponents<Graph>`, `BasicIncrementalKCore<Graph>`, `BasicIncrementalPageRank<Graph>`, and `BasicIncrementalTriangleCount<Graph>` can run over compatible graph representations.

Hot traversal paths use callback/span-style adjacency access rather than materializing a `std::vector` for each vertex visit. `CsrGraph` maintains forward and reverse CSR, enabling the same algorithm implementation to run against mutable and read-optimized storage. The backend BFS benchmark gates timing output on identical distance vectors.

See [Storage-independent graph algorithm contract](docs/graph-abstraction.md).

## Correctness and reproducibility

CI covers:

- Ubuntu and macOS builds and tests
- Linux ASan/UBSan
- graph mutation and storage consistency
- incremental-vs-full differential testing
- SIMD/scalar agreement
- Python interoperability with NumPy, SciPy, and Arrow
- dataset provenance and checksum verification
- benchmark contracts and independent reference checks

Experiments use explicit thread settings, repeated measurements, exactness gates, environment capture, and retained GitHub Actions artifacts. Hosted-runner timing is treated as reproducible evidence within a recorded environment, not as a hardware-independent performance guarantee.

## Methodology and documentation

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

Enable Python bindings with:

```bash
cmake -S . -B build -DVELOGRAPHX_BUILD_PYTHON=ON
```

## License

Licensed under the [Apache License 2.0](LICENSE).
