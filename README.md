# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact incremental analytics on changing graphs.** It combines compact mutable graph storage, localized repair, adaptive full recomputation, CPU-aware execution, and reproducible benchmarking.

The central systems question is: **when should a graph update be repaired incrementally, and when is full recomputation cheaper?** VeloGraphX does not claim novelty from the individual graph algorithms themselves.

## Core architecture

```text
update stream
    ↓
dynamic graph storage
    ↓
work estimation / adaptive decision
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
| Adaptive execution | Update-density preflight, affected-work budgeting, incremental-vs-full fallback |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20, pybind11, NumPy, SciPy CSR, Apache Arrow |
| Reproducibility | Checksum-pinned datasets, pinned competitors, exactness gates, environment capture, retained artifacts |

## Key evidence

Results below are **hosted-CI engineering measurements with independent exactness checks**, not universal performance claims.

### Exact dynamic BFS vs NetworKit

NetworKit 11.2.1 is compared in native C++ on the same hosted runner with one thread, identical update streams, and independent full-BFS validation after every batch.

| Dataset | VeloGraphX | NetworKit 11.2.1 | VX/NK |
| --- | ---: | ---: | ---: |
| `web-Google` | **31.512 ms** | 41.293 ms | **0.764x** |
| `ca-GrQc` | 0.1110 ms | **0.0808 ms** | **1.374x** |
| `soc-Epinions1` (3 roots × 5 paired runs) | 1.794 ms | **1.152 ms** | **1.582x** |

On the canonical `web-Google` campaign, VeloGraphX had **23.6% lower answer-ready batch latency** than NetworKit. The focused `ca-GrQc` optimization campaign reduced VeloGraphX latency from **0.3134 ms to 0.1110 ms (~64.6%)** while preserving exact results.

The checksum-pinned directed `soc-Epinions1` family has **75,879 vertices and 508,837 edges**. Three deterministic reachability-screened roots and five paired repetitions per root produced **15/15 exact VeloGraphX/NetworKit pairs**; the mean paired VX/NK ratio was **1.582x**. Evidence run `33303152827`, artifact `9729665685`.

Canonical two-dataset campaign: GitHub Actions `33301190847`, artifact `9729078197`.

### Adaptive incremental-vs-recompute policy

A separate exact experiment compares four strategies on dynamic BFS:

- always incremental repair;
- always full recomputation;
- a simple fixed update threshold;
- VeloGraphX's two-stage adaptive policy, using a cheap update-density preflight followed by an affected-work repair budget.

The validation covers **3 checksum-pinned graph families × 3 roots × 3 update regimes × 5 repetitions**, or **27 regimes**. Every measured policy result passed independent exact full-BFS verification.

Across those 27 regimes, the adaptive policy was **3.78% above the fastest policy on average** and was itself fastest in **10/27 regimes**. The experiment also exposes genuine crossover behavior: incremental repair is preferable in many small/medium regimes, while recomputation or threshold decisions become preferable in some larger regimes.

The predeclared worst-case acceptance bound was 1.30x the fastest policy. The adaptive policy's worst regime was **1.334x** (`web-Google`, root `391806`, batch `6144`), so the experiment **did not pass the promotion gate**. The candidate therefore remains research evidence rather than a claim of a solved or universally superior selector. Validation run `33312852351`, commit `bb9f0162f20551f2da39212daa0e9c61cd9609bc`.

### Exact dynamic triangles

On `com-LiveJournal` (34.7M base edges), exact incremental maintenance versus exact full recomputation achieved:

| Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: |
| 0.01% | 4.066 ms | 13.126 s | **3,228.66x** |
| 0.1% | 34.709 ms | 13.288 s | **382.85x** |
| 1% | 415.377 ms | 14.119 s | **33.99x** |

Exact validation also completed on `com-Orkut` with **117,185,083 edges** and **627,584,181 initial triangles**.

Against the pinned exact `GoldenCounter` component from public SIGMOD 2021 source code, **15/15 results matched exactly**, with **3.48x–40.95x lower latency** on the measured workload. This comparison is with the exact reference component, not the paper's approximate SWTC algorithm.

A separate native `web-Google` campaign measured RisGraph at **31.333 ms** versus VeloGraphX at **59.658 ms**. It ran on a different hosted runner from the NetworKit campaign, so these measurements are not combined into a three-system ranking.

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

The repository demonstrates exact dynamic graph execution, compact mutable storage, localized maintenance, adaptive recomputation experiments, and reproducible external comparisons. It does **not** establish universal superiority, production maturity, or a universally optimal adaptive policy. Stronger publication claims require broader graph/update families, dedicated hardware, same-machine competitor campaigns beyond NetworKit, multicore/NUMA evaluation, hardware counters, and independent reproduction.

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
