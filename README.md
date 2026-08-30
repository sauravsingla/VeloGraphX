# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for incremental analytics on changing graphs.** It combines dynamic graph storage, localized algorithm repair, adaptive full recomputation, CPU-aware execution, and reproducible benchmarking.

Its central question is: **when should a graph update be repaired incrementally, and when is full recomputation cheaper?** The project focuses on the systems architecture around that decision rather than claiming novelty from reimplementing individual graph algorithms.

## Architecture

```text
update stream
    ↓
dynamic graph storage
    ↓
affected-work estimation
    ↓
localized repair  ↔  full recomputation
    ↓
CPU execution layer
    ↓
maintained result
```

## Implemented scope

| Area | Current implementation |
| --- | --- |
| Incremental analytics | BFS / unweighted SSSP, weighted SSSP, exact triangles, connected components, k-core, PageRank repair |
| Dynamic storage | Segmented CSR, packed deltas, sparse row patches, reverse adjacency, validated consolidation |
| Adaptive execution | Affected-work estimation and incremental-vs-full fallback |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20 core, pybind11, NumPy, SciPy CSR and Apache Arrow ingestion |
| Reproducibility | Dataset checksums, pinned competitor revisions, correctness gates, environment capture and retained artifacts |

## Selected evidence

All numbers below are **hosted-CI engineering measurements with correctness checks**, not universal performance claims. Full methodology and raw evidence are linked below.

### Exact dynamic triangle maintenance

On `com-LiveJournal` (34.7M base edges), exact incremental maintenance versus exact full recomputation produced:

| Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: |
| 0.01% | 4.066 ms | 13.126 s | **3,228.66x** |
| 0.1% | 34.709 ms | 13.288 s | **382.85x** |
| 1% | 415.377 ms | 14.119 s | **33.99x** |

Exact validation also completed on `com-Orkut` with **117,185,083 edges** and **627,584,181 initial triangles**.

### Published exact triangle reference

Against the pinned exact `GoldenCounter` component from public SIGMOD 2021 source code, all **15/15 results matched exactly**. On the measured workload, VeloGraphX showed **3.48x–40.95x lower latency**, depending on update size. This comparison is only with that exact reference component, not with the paper's approximate SWTC algorithm.

### Native dynamic-BFS baselines

The latest NetworKit campaign uses native C++, the same hosted runner, one thread, five paired repetitions per dataset, identical update streams and exact full-BFS validation after every batch.

| Dataset | VeloGraphX | NetworKit 11.2.1 | Paired VX/NK ratio |
| --- | ---: | ---: | ---: |
| `web-Google` | **39.38 ms** | **41.53 ms** | **0.948x** |
| `ca-GrQc` | **0.3134 ms** | **0.0887 ms** | **3.53x** |

On `web-Google`, VeloGraphX was about **5.2% faster than NetworKit** in this same-run hosted-CI campaign. On `ca-GrQc`, VeloGraphX remains materially slower, although mean batch latency remains substantially below the earlier clean **0.3884 ms** baseline. All five repetitions on both datasets passed exact correctness and nontrivial-reachability gates. Latest campaign uses VeloGraphX commit `7fd75c0...` and retained artifact `9727662196`.

A separate native `web-Google` campaign measured RisGraph at **31.333 ms** versus VeloGraphX at **59.658 ms**. Because the RisGraph and NetworKit campaigns ran on different hosted runners, their absolute times are not combined into a three-system ranking.

## Correctness and reproducibility

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, dynamic mutation correctness, differential incremental-vs-full checks, storage consistency, SIMD/scalar agreement, scheduler and NUMA behavior, compression, I/O, Python interoperability, dataset checksums and benchmark contracts.

Key evidence:

- [External dynamic baselines](docs/external-dynamic-baselines.md)
- [CI-scale evidence](docs/ci-scale-evidence.md)
- [Published exact baseline](docs/same-run-published-baseline.md)
- [Storage evidence](docs/storage-ab-evidence.md)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [Current limitations](docs/limitations.md)

## Research boundary

VeloGraphX demonstrates exact large-graph execution, incremental maintenance, dynamic storage, adaptive recomputation and reproducible external comparisons. It does **not** establish universal superiority or production maturity.

Stronger publication claims still require dedicated hardware, more graph families and roots, repeated multi-seed workloads, same-machine native comparisons across serious same-semantics systems, physical-core scaling, multi-socket NUMA experiments, hardware counters and independent reproduction.

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Python

```bash
cmake -S . -B build-python \
  -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_PYTHON=ON
cmake --build build-python -j
```

## Contributing

Contributions are welcome across dynamic algorithms, storage, correctness, CPU performance, benchmarking and interoperability. Performance-sensitive changes should preserve the relevant correctness contract and include reproducible measurements.

## License

Licensed under the [Apache License 2.0](LICENSE).
