# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![Version](https://img.shields.io/badge/version-0.7.0-blue.svg)](CITATION.cff)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

**VeloGraphX is a C++20 CPU engine for exact analytics on changing graphs.** It combines mutable graph storage, localized incremental repair, and adaptive fallback to full recomputation when repair becomes more expensive.

The project focuses on **exactness, crossover-aware execution, storage independence, and reproducible system comparisons**.

## Highlights

- **Exact dynamic analytics:** BFS/unweighted SSSP, weighted SSSP, connected components, triangle count, k-core and PageRank paths.
- **Adaptive repair vs recompute:** decisions use update fraction, affected work, graph scale, root locality and observed cost.
- **Storage-independent algorithms:** the same graph-access contract supports mutable storage, CSR and foreign graph representations.
- **CPU systems runtime:** multicore execution, SIMD intersections, NUMA-aware policies, compression, partition caching and asynchronous partition loading.
- **Auditable benchmarking:** pinned competitors/datasets, raw samples, correctness gates, retained artifacts and negative results.
- **C++ first, Python optional:** native hot paths remain in C++; pybind11 bindings can be enabled at build time.

## Results at a glance

> GitHub-hosted numbers are **reproducible engineering evidence, not publication-grade hardware claims**.

| Evidence | Verified result |
| --- | --- |
| Dynamic exactness stress | **2,000,000 updates; 0 BFS / 0 triangle mismatches** |
| Adaptive BFS selector | **108/108 exact; 1.66% mean overhead from regime-best** |
| VeloGraphX vs GraphBolt/DZiG | **15.35× / 4.28× / 2.33× faster** on tiny / medium / large hosted update regimes |
| VeloGraphX vs NetworKit vs RisGraph | **91/91 exact; 45 / 27 / 19 raw-policy wins** |
| Dynamic BFS vs NetworKit | `web-Google`: **VeloGraphX ~1.38× faster**; `ca-GrQc`: **NetworKit ~1.35× faster** |
| Static BFS vs GAP / LAGraph | **VeloGraphX fastest** in tested 1T and 4T BFS cases |
| Static SSSP vs GAP / LAGraph | **GAP fastest** in tested 1T and 4T SSSP cases |
| Multicore | BFS **2.74×**, CC **2.50×**, triangles **2.24×** at 4 threads |
| Compression | **3.25×–3.78× smaller**, with a current BFS traversal cost |
| Public scale exercised | **875,713 vertices / 5,105,039 edges** (`web-Google`) |

### VeloGraphX vs GraphBolt/DZiG — dynamic BFS

Same deterministic directed graph, root, update workload and one-worker configuration. Both timings cover **graph mutation + incremental answer maintenance**; GraphBolt stream-read time is excluded.

| Updates | VeloGraphX median | GraphBolt/DZiG median | Outcome |
| ---: | ---: | ---: | ---: |
| 400 | **83.45 µs** | 1,281 µs | **15.35× faster** |
| 4,000 | **1,427.86 µs** | 6,106 µs | **4.28× faster** |
| 20,000 | **6,875.96 µs** | 16,012 µs | **2.33× faster** |

All VeloGraphX results were exact; every GraphBolt result passed an independent fresh-recompute reachability verifier. Official GraphBolt is pinned to `2d56f39cb17c85d624bee6a63f8fc34a8f149a36` with `CILK_NWORKERS=1`.

**GAPBS context:** v1.5 fresh-BFS kernel medians were **750 / 770 / 870 µs** on the already-materialized post-update graphs. GAPBS excludes mutation and graph materialization, so these values are a **static recompute reference, not a direct dynamic-system comparison**.

Evidence: run `33713976273`, artifact `9877875056`. See [GraphBolt/DZiG + GAPBS contract](docs/graphbolt-dzig-gap-benchmark-contract.md).

### VeloGraphX vs NetworKit vs RisGraph — dynamic BFS

The hosted [three-system campaign](docs/three-system-dynamic-bfs-campaign.md) uses the same machine, graph, root, update stream and one-thread configuration across five datasets and update fractions from **0.0001% to 10%**.

| System | Fastest configurations | Share |
| --- | ---: | ---: |
| **VeloGraphX** | **45** | **49.5%** |
| RisGraph | 27 | 29.7% |
| NetworKit | 19 | 20.9% |

All **91 configurations passed exactness gates**. Crossover behavior is retained: on `web-Google` at **0.001%** updates, RisGraph (~76–95 µs) and NetworKit (~100–152 µs) beat VeloGraphX (~1.7 ms raw incremental repair). At **5–10%**, adaptive VeloGraphX becomes substantially faster on the tested `web-Google` workload.

Evidence: run `33578440940`. NetworKit revision `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`; RisGraph revision `4e77f77...`.

### Other comparison evidence

| Comparison | Result | Evidence |
| --- | --- | --- |
| NetworKit `DynBFS` | VX **27.182 ms** vs NK 37.458 ms on `web-Google`; VX 0.1115 ms vs NK **0.08274 ms** on `ca-GrQc`; **30/30 exact** | run `33542995289`, artifact `9814639042` |
| GAP v1.5 / LAGraph — BFS | VX **0.425 ms** (1T), **0.406 ms** (4T); GAP 0.790/0.830 ms; LAGraph 4.0/4.8 ms | run `33418520303`, artifact `9768499895` |
| GAP v1.5 / LAGraph — SSSP | GAP **1.060 ms** (1T), **1.290 ms** (4T); VX 8.982/9.003 ms | same run |
| CSR storage | CSR full BFS ~**2.08×–2.14× faster** than VeloGraphX mutable storage in tested cases | run `33475389747`, artifact `9787994251` |
| Teseo adapter | VeloGraphX `DynamicGraph` traversal ~**36×–44× faster** than the tested Teseo iterator adapter using the same VX BFS | same run |

The Teseo result is a **storage-interface experiment, not a comparison against Teseo's own graph algorithms**.

## Architecture

```text
update batch
   ↓
mutable graph: base CSR + deltas / row patches + consolidation
   ↓
affected work + observed cost + root locality + update fraction
   ↓
localized exact repair  ← selector →  full recomputation
   ↓
exact maintained result
```

## Quick start

Requires **CMake ≥ 3.20** and a **C++20 compiler**.

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/velographx_example
./build/velographx_dynamic_example
```

The default build currently defines **29 CTest targets** plus benchmark executables.

### Minimal C++ API

```cpp
#include "velographx/algorithms.hpp"

velographx::CsrGraph graph({{0,1}, {1,2}, {2,3}}, false);
auto distance = velographx::bfs_distances(graph, 0);
auto triangles = velographx::triangle_count(graph);
```

For dynamic updates, see [`examples/dynamic_transactions.cpp`](examples/dynamic_transactions.cpp). Optional Python bindings are enabled with `-DVELOGRAPHX_BUILD_PYTHON=ON`; see [`python/README.md`](python/README.md).

## Reproduce the 2M-update exactness test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_TESTS=OFF -DVELOGRAPHX_BUILD_BENCHMARKS=OFF
cmake --build build --target velographx -j 2
c++ -O3 -DNDEBUG -std=c++20 -Iinclude benchmarks/exactness_stress.cpp \
  build/libvelographx.a -pthread -o build/exactness_stress
./build/exactness_stress 2000000 256
```

## Evidence & documentation

- [Architecture](docs/architecture.md) · [Dynamic storage](docs/dynamic-storage.md) · [Graph abstraction](docs/graph-abstraction.md)
- [Benchmark methodology](docs/benchmark-methodology.md) · [Competitor benchmarking](docs/competitor-benchmarking.md) · [Ablation study](docs/ablation-study.md)
- [GraphBolt/DZiG + GAPBS contract](docs/graphbolt-dzig-gap-benchmark-contract.md) · [Three-system campaign](docs/three-system-dynamic-bfs-campaign.md)
- [Canonical publication campaign](docs/canonical-publication-campaign.md) · [Controlled-hardware execution](docs/controlled-hardware-execution.md) · [Limitations](docs/limitations.md)

## Evidence boundary / next step

The hosted campaigns establish correctness, reproducibility and crossover behavior, but shared GitHub runners are noisy and hardware can vary. **Controlled-hardware publication tables remain pending.** The canonical campaign is designed for pinned datasets and hardware, **1/2/4/8/16/32-thread scaling, NUMA placement, hardware counters and larger real-world/R-MAT workloads**.

Known negative results are intentionally preserved: competitors win some small-update regimes; GAP is much faster on the tested static SSSP workload; CSR is faster for full recompute; and compression currently trades traversal speed for memory reduction.

## Project

Current project version: **0.7.0**. Research citation metadata is available in [`CITATION.cff`](CITATION.cff). No GitHub release is currently published, so cite the repository and the relevant commit/version when using current results.

Apache-2.0 licensed. See [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), [SECURITY.md](SECURITY.md), and [CHANGELOG.md](CHANGELOG.md).
