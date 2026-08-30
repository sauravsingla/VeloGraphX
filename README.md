# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact incremental analytics on changing graphs.** It combines dynamic graph storage, localized repair, adaptive full recomputation, CPU-aware execution, and reproducible benchmarking.

Its central systems question is: **when should an update be repaired incrementally, and when is full recomputation cheaper?** The project does not claim novelty from reimplementing individual graph algorithms.

## Core architecture

```text
update stream
    ↓
dynamic graph storage
    ↓
affected-work estimation
    ↓
localized repair  ↔  full recomputation
    ↓
CPU execution
    ↓
exact maintained result
```

| Area | Implementation |
| --- | --- |
| Incremental analytics | BFS / unweighted SSSP, weighted SSSP, exact triangles, connected components, k-core, PageRank repair |
| Dynamic storage | Segmented CSR, packed deltas, sparse row patches, reverse adjacency, validated consolidation |
| Adaptive execution | Affected-work estimation and incremental-vs-full fallback |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20, pybind11, NumPy, SciPy CSR, Apache Arrow |
| Reproducibility | Dataset checksums, pinned competitors, exactness gates, environment capture, retained artifacts |

## Key evidence

The results below are **hosted-CI engineering measurements with exact correctness checks**, not universal performance claims.

### Native exact dynamic BFS vs NetworKit

NetworKit 11.2.1 is compared in native C++ on the same hosted runner with one thread, five paired repetitions per dataset, identical update streams, and independent exact full-BFS validation after every batch.

| Dataset | VeloGraphX | NetworKit 11.2.1 | VX/NK |
| --- | ---: | ---: | ---: |
| `web-Google` | **31.512 ms** | 41.293 ms | **0.764x** |
| `ca-GrQc` | 0.1110 ms | **0.0808 ms** | **1.374x** |

VeloGraphX was **23.6% lower-latency on web-Google** in this same-run campaign. The focused small-graph campaign reduced `ca-GrQc` from **0.3134 ms to 0.1110 ms (~64.6%)** while preserving exact results. All **5/5 repetitions on both datasets were exact**.

Supplementary three-root exact testing measured `ca-GrQc` at **103.546–108.345 µs** and `web-Google` at **22.691–30.986 ms**, with every selected root passing the reachability and correctness gates.

Canonical campaign: GitHub Actions `33301190847`, VeloGraphX head `3c1f7448897ffdca227a261c61bd49751e42fa5f`, artifact `9729078197`.

### Exact dynamic triangles

On `com-LiveJournal` (34.7M base edges), exact incremental maintenance versus exact full recomputation achieved:

| Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: |
| 0.01% | 4.066 ms | 13.126 s | **3,228.66x** |
| 0.1% | 34.709 ms | 13.288 s | **382.85x** |
| 1% | 415.377 ms | 14.119 s | **33.99x** |

Exact validation also completed on `com-Orkut` with **117,185,083 edges** and **627,584,181 initial triangles**.

Against the pinned exact `GoldenCounter` component from public SIGMOD 2021 source code, **15/15 results matched exactly**, with **3.48x–40.95x lower latency** on the measured workload. This comparison is with the exact reference component, not the paper's approximate SWTC algorithm.

A separate native `web-Google` campaign measured RisGraph at **31.333 ms** versus VeloGraphX at **59.658 ms**. It ran on a different hosted runner from the NetworKit campaign, so the results are intentionally not combined into a three-system ranking.

## Correctness and reproducibility

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, dynamic mutation and incremental-vs-full differential correctness, storage consistency, SIMD/scalar agreement, scheduler/NUMA behavior, Python interoperability, dataset provenance, and benchmark contracts.

Detailed evidence and methodology:

- [External dynamic baselines](docs/external-dynamic-baselines.md)
- [CI-scale evidence](docs/ci-scale-evidence.md)
- [Published exact baseline](docs/same-run-published-baseline.md)
- [Storage evidence](docs/storage-ab-evidence.md)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [Current limitations](docs/limitations.md)

## Research boundary

The repository provides exact large-graph execution, dynamic storage, localized maintenance, adaptive recomputation, and reproducible external comparisons. It does **not** establish universal superiority or production maturity. Publication-grade performance claims still require dedicated hardware, broader graph/update families, same-machine native competitor campaigns, multicore/NUMA experiments, hardware counters, and independent reproduction.

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Python bindings can be enabled with `-DVELOGRAPHX_BUILD_PYTHON=ON`.

## License

Licensed under the [Apache License 2.0](LICENSE).
