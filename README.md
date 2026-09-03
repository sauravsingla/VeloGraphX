# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

**VeloGraphX is a C++20 CPU engine for exact analytics on changing graphs.**

It maintains results with localized incremental repair when that is cheaper, and switches to full recomputation when repair becomes too expensive. The emphasis is **exactness, measurable crossover behavior, and reproducible system comparisons** rather than inventing new BFS/SSSP/triangle algorithms.

## Why VeloGraphX

- **Exact dynamic analytics:** incremental outputs are checked against independent full recomputation.
- **Adaptive repair vs recompute:** the selector uses scale, root locality, update fraction, affected work, and observed cost.
- **Storage-independent algorithms:** a C++20 graph-access contract lets the same algorithm run over mutable storage, CSR, and foreign graph representations.
- **Systems-oriented runtime:** multicore execution, SIMD intersections, NUMA-aware policies, compression, partition caching, and asynchronous partition loading.
- **Auditable benchmarks:** pinned datasets/versions, raw samples, correctness gates, retained artifacts, and explicit negative results.

See [graph abstraction](docs/graph-abstraction.md), [benchmark methodology](docs/benchmark-methodology.md), and [related-work positioning](docs/related-work-positioning.md).

## Results at a glance

GitHub-hosted measurements below are **engineering evidence, not publication-grade hardware claims**.

| Evidence | Result |
| --- | ---: |
| Dynamic exactness stress | **2,000,000 updates; 0 BFS / 0 triangle mismatches** |
| Adaptive BFS selector | **108/108 exact; 1.66% mean overhead from regime-best** |
| `web-Google` adaptive BFS | **1.17% mean overhead** |
| Multicore BFS workload | **2.74× at 4 threads (~69% efficiency)** |
| Connected components | **2.50× at 4 threads (~62%)** |
| Triangle count | **2.24× at 4 threads (~56%)** |
| Adjacency compression | **3.25×–3.78× smaller** |
| Public graph scale exercised | **875,713 vertices / 5,105,039 edges** (`web-Google`) |

Adaptive-selector campaign: run `33471125549`, artifact `9786648824`. Exactness/multicore/compression: run `33467637208`.

## VeloGraphX vs Other Graph Systems

All published comparisons below use matched workloads and correctness checks. Results where another system wins are reported explicitly.

### VeloGraphX vs NetworKit 11.2.1 — Dynamic BFS

Native C++, one OpenMP thread, same machine, update stream and roots; five paired repetitions per root. Every execution must match a fresh full BFS.

| Dataset | VeloGraphX | NetworKit `DynBFS` | VX / NK | Winner |
| --- | ---: | ---: | ---: | --- |
| `web-Google` | **27.182 ms** | 37.458 ms | **0.730×** | **VeloGraphX (~1.38× faster)** |
| `ca-GrQc` | 0.1115 ms | **0.08274 ms** | 1.350× | **NetworKit (~1.35× faster)** |

VeloGraphX wins all three tested roots on `web-Google`; NetworKit wins all three on the smaller `ca-GrQc`. All **30 paired executions passed exact full-BFS verification**.

NetworKit revision `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`; run `33542995289`, artifact `9814639042`.

### VeloGraphX vs GAP v1.5 vs LAGraph v1.3.x — Static BFS / SSSP

Same hosted runner, graph/source/thread count, five repetitions, kernel-only timing, pinned competitor versions and correctness checks.

| Kernel | Threads | VeloGraphX | GAP v1.5 | LAGraph v1.3.x | Winner |
| --- | ---: | ---: | ---: | ---: | --- |
| BFS | 1 | **0.425 ms** | 0.790 ms | 4.000 ms | **VeloGraphX** |
| BFS | 4 | **0.406 ms** | 0.830 ms | 4.800 ms | **VeloGraphX** |
| SSSP | 1 | 8.982 ms | **1.060 ms** | 23.500 ms | **GAP** |
| SSSP | 4 | 9.003 ms | **1.290 ms** | 27.100 ms | **GAP** |

VeloGraphX wins BFS on this workload; **GAP remains substantially faster for SSSP**.

Pinned contract: SuiteSparse:GraphBLAS `v10.5.0`, LAGraph `d01064de77b606473744b99f63b1487963556194`, GAP `v1.5`. Run `33418520303`, artifact `9768499895`.

### VeloGraphX storage vs CSR vs Teseo adapter — Same BFS algorithm

`BasicIncrementalBFS::recompute()` is unchanged; only graph representation changes. Construction is outside the timer and full distance vectors must match.

| Vertices / edges | VeloGraphX `DynamicGraph` | `CsrGraph` | Teseo adapter |
| --- | ---: | ---: | ---: |
| 8,192 / 32,768 | 120.434 µs | **57.988 µs** | 4,341.401 µs |
| 32,768 / 131,072 | 500.383 µs | **234.284 µs** | 21,854.866 µs |

CSR is about **2.08×–2.14× faster** than VeloGraphX mutable storage for full recomputation on these cases. With the same VeloGraphX BFS implementation, `DynamicGraph` traversal is about **36×–44× faster than the Teseo iterator adapter**.

This is a **storage-interface experiment, not a claim against Teseo's own algorithms**. Teseo commit `2c37c2831c4d2acaaa838a86e1318363ce68c45b`; run `33475389747`, artifact `9787994251`. See [Teseo evidence](docs/teseo-storage-evidence.md).

## Next comparison campaigns

The repository contains broader benchmark contracts whose final comparative numbers are intentionally withheld until their full artifacts pass audit.

**VeloGraphX vs NetworKit vs RisGraph.** The [three-system dynamic BFS campaign](docs/three-system-dynamic-bfs-campaign.md) uses the same machine, roots, streams and timed envelope across `web-Google`, `soc-Epinions1`, `roadNet-CA`, R-MAT and `com-LiveJournal`, sweeping update fractions from **0.0001% to 10%**. Competitor wins and selector losses are retained rather than filtered.

**VeloGraphX vs GraphBolt/DZiG + GAPBS.** The [GraphBolt/DZiG contract](docs/graphbolt-dzig-gap-benchmark-contract.md) pins the official GraphBolt artifact revision, generates a deterministic native update stream, parses native timing/work counters, and independently verifies GraphBolt BFS reachability. Hosted CI validates the contract; comparative performance numbers require the controlled dedicated runner.

The [canonical publication campaign](docs/canonical-publication-campaign.md) is the path for controlled 1/2/4/8/16/32-thread scaling, NUMA placement, hardware counters, checksum-pinned datasets and larger R-MAT/real-world workloads.

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

Implemented paths include BFS/unweighted SSSP, weighted SSSP, connected components, triangle count, k-core and PageRank-related analytics. Generic unweighted algorithms use the graph-access abstraction; weighted SSSP currently keeps a specialized weighted-graph contract while sharing the Dijkstra engine.

## Quick start

Requires CMake ≥ 3.20 and a C++20 compiler.

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/velographx_example
./build/velographx_dynamic_example
```

The default build includes **29 CTest targets** plus benchmark executables. One test uses an ADL-only foreign graph type to guard against accidental storage coupling.

## Reproduce the 2M-update exactness test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVELOGRAPHX_BUILD_TESTS=OFF -DVELOGRAPHX_BUILD_BENCHMARKS=OFF
cmake --build build --target velographx -j 2
c++ -O3 -DNDEBUG -std=c++20 -Iinclude benchmarks/exactness_stress.cpp build/libvelographx.a -pthread -o build/exactness_stress
./build/exactness_stress 2000000 256
```

For benchmark interpretation and reproducibility details, see [benchmark methodology](docs/benchmark-methodology.md), [native competitors](docs/hosted-native-competitors.md), [ablation study](docs/ablation-study.md), and [limitations](docs/limitations.md).

## Current limitations

- Hosted CI demonstrates reproducibility and engineering behavior; it is not controlled-hardware publication evidence.
- The adaptive selector averages **1.66% overhead from regime-best** on the tested 36 root/regime configurations, not universally.
- Teseo/Sortledton experiments isolate the BFS/storage interface and are not full-system comparisons.
- Compression saves memory but currently slows BFS traversal (**3.8×–5.7×** in the hosted compression campaign).
- Dedicated-runner NUMA, hardware-counter, near-memory-capacity and final multi-system publication results remain pending.

See [limitations](docs/limitations.md) for the full evidence boundary.

## License / contributing

Apache License 2.0 — see [LICENSE](LICENSE). Contributions should be small, tested, and benchmarked when performance claims change; see [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
