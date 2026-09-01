# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

## What it is

VeloGraphX is a C++20 CPU engine for **exact analytics on changing graphs**. It maintains graph state with localized repair when updates are cheap enough, but can switch to full recomputation when repair becomes more expensive. The goal is predictable exactness with measurable update/recompute trade-offs, not new BFS/SSSP/triangle algorithms.

## Novelty

- Couples **compact mutable storage + exact localized repair + measured affected work + repair-vs-recompute selection** in one CPU-native system.
- Makes the incremental/full crossover explicit: the selector owns the decision before excessive repair work is incurred.
- Treats **exactness as a benchmark gate**: incremental outputs are checked against independent full recomputation/reference implementations.
- Uses scale, root-locality, affected-work and observed-cost signals; policy quality is evaluated on pinned datasets, multiple roots/regimes and retained artifacts.
- Separates algorithms from graph representation through a **non-intrusive C++20 graph-access contract**, so the same algorithm can be exercised over mutable storage, read-optimised CSR and foreign graph representations.

See [related-work positioning](docs/related-work-positioning.md), the [graph abstraction](docs/graph-abstraction.md), and the [ablation study](docs/ablation-study.md).

## Results

GitHub-hosted results below are **engineering evidence, not publication-grade hardware claims**.

| Evidence | Result | Scope |
| --- | ---: | --- |
| Dynamic exactness stress | **2,000,000 updates; 0 BFS / 0 triangle mismatches** | 6 adversarial scenarios; independent check after every batch |
| Refined adaptive BFS | **108/108 exact; 1.66% mean overhead from regime-best** | 3 datasets × 4 roots × 3 regimes × 3 reps |
| `web-Google` adaptive BFS | **1.17% mean overhead** | down from 25.8% in the pre-refinement campaign |
| External storage swap | **exact across DynamicGraph / CSR / Teseo** | identical `BasicIncrementalBFS::recompute`, 5 reps |
| Multicore BFS workload | **2.74× at 4 threads (~69% efficiency)** | 50K vertices, degree 8, 32 independent tasks, median of 5 reps |
| Connected-components workload | **2.50× at 4 threads (~62%)** | same workload contract |
| Triangle-count workload | **2.24× at 4 threads (~56%)** | same workload contract |
| Variable-byte adjacency compression | **3.25×–3.78× smaller** | 100K vertices, degrees 4/8/16 |
| Compressed BFS trade-off | **3.8×–5.7× slower than raw traversal** | same end-to-end compression campaign |
| Public dynamic graph scale | **875,713 vertices / 5,105,039 edges** | pinned `web-Google` normalization |

The refined selector uses root out/in locality, guarded affected-work fallback for low-degree roots on large graphs, update-fraction guards and the existing observed-cost model. Across the 36 root/regime configurations its dataset-level mean overhead was **2.70% (`ca-GrQc`)**, **1.10% (`soc-Epinions1`)**, and **1.17% (`web-Google`)**; worst observed regime overhead was **10.67%**. Run `33471125549`, artifact `9786648824`. Exactness stress/multicore/compression evidence: run `33467637208`.

## Comparisons

### Static native BFS / SSSP

Same hosted runner, graph/source/thread count, five repetitions, kernel-only timing, pinned external versions and correctness checks. LAGraph/SuiteSparse:GraphBLAS and GAP follow the repository's Davis benchmark contract.

| Kernel | Threads | VeloGraphX | GAP v1.5 | LAGraph v1.3.x |
| --- | ---: | ---: | ---: | ---: |
| BFS | 1 | **0.425 ms** | 0.790 ms | 4.000 ms |
| BFS | 4 | **0.406 ms** | 0.830 ms | 4.800 ms |
| SSSP | 1 | 8.982 ms | **1.060 ms** | 23.500 ms |
| SSSP | 4 | 9.003 ms | **1.290 ms** | 27.100 ms |

VeloGraphX wins BFS on this workload; **GAP remains substantially faster for SSSP**. Pinned contract: SuiteSparse:GraphBLAS `v10.5.0`, LAGraph `d01064de77b606473744b99f63b1487963556194`, GAP `v1.5`. Run `33418520303`, artifact `9768499895`.

### Dynamic BFS vs NetworKit 11.2.1

Native C++, one OpenMP thread, same machine/update stream, five paired repetitions, exactness and nontrivial-reachability gates.

| Dataset | VeloGraphX | NetworKit `DynBFS` | VX / NK |
| --- | ---: | ---: | ---: |
| `web-Google` | **36.305 ms** | 45.845 ms | **0.788×** |
| `ca-GrQc` | 0.1193 ms | **0.0892 ms** | **1.333×** |

VeloGraphX wins the larger `web-Google` case but **loses on `ca-GrQc`**. NetworKit revision `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`; run `33301190847`, artifact `9766977170`.

### Same-algorithm storage swap: Teseo

This experiment keeps `BasicIncrementalBFS::recompute()` fixed and changes only the graph representation. Graph construction is outside the timer; five recomputations are run and the median is reported. Full distance vectors must match.

| Vertices / edges | `DynamicGraph` | `CsrGraph` | Teseo adapter | Exact |
| --- | ---: | ---: | ---: | :---: |
| 8,192 / 32,768 | 120.434 µs | **57.988 µs** | 4,341.401 µs | yes |
| 32,768 / 131,072 | 500.383 µs | **234.284 µs** | 21,854.866 µs | yes |

Read-optimised CSR is about **2.08×–2.14× faster** than the mutable VeloGraphX representation for full recomputation on these cases. Using the same VeloGraphX BFS implementation, `DynamicGraph` traversal is about **36×–44× faster than the Teseo iterator adapter**. This is deliberately a **storage-interface experiment, not a system-level claim against Teseo's own algorithms or capabilities**. Teseo commit `2c37c2831c4d2acaaa838a86e1318363ce68c45b`; run `33475389747`, artifact `9787994251`. See [Teseo storage evidence](docs/teseo-storage-evidence.md).

RisGraph and other dynamic systems remain relevant prior work; results from different runners or incompatible semantics are intentionally **not merged into same-semantics tables**.

## How it works

```text
update batch
   ↓
mutable graph: base CSR + deltas/row patches + consolidation
   ↓
affected-work / cost + root-locality signals
   ↓
localized exact repair  ← selector →  full recomputation
   ↓
exact maintained result
```

Implemented analytics include BFS/unweighted SSSP, weighted SSSP, connected components, triangle count, k-core and PageRank-related paths. Generic unweighted algorithms use the C++20 graph-access layer rather than a concrete storage type; the weighted SSSP path currently retains a specialised weighted-graph contract while sharing the common Dijkstra engine. Runtime/storage code also includes SIMD intersections, work stealing, NUMA-aware policies, compression, partition caching and asynchronous partition loading.

## Quick Start

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

The default build includes **29 CTest targets** plus benchmark executables. One test uses an ADL-only foreign graph type to prevent accidental coupling of generic algorithms back to VeloGraphX storage members.

## Reproduce

The 2M-update exactness headline can be reproduced locally without dataset downloads:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVELOGRAPHX_BUILD_TESTS=OFF -DVELOGRAPHX_BUILD_BENCHMARKS=OFF
cmake --build build --target velographx -j 2
c++ -O3 -DNDEBUG -std=c++20 -Iinclude benchmarks/exactness_stress.cpp build/libvelographx.a -pthread -o build/exactness_stress
./build/exactness_stress 2000000 256
```

The refined selector, native competitor and Teseo storage-swap campaigns are encoded as workflows so version pins, correctness gates and artifacts stay with the run.

```bash
# Requires an authenticated GitHub CLI.
gh workflow run adaptive-selector-refinement.yml --ref main
gh workflow run hosted-native-competitors.yml --ref main
gh workflow run external-teseo-storage.yml --ref main

gh run list --workflow adaptive-selector-refinement.yml --limit 1
gh run list --workflow hosted-native-competitors.yml --limit 1
gh run list --workflow external-teseo-storage.yml --limit 1
```

Benchmark contracts and interpretation rules: [benchmark methodology](docs/benchmark-methodology.md), [graph abstraction](docs/graph-abstraction.md), [native competitors](docs/hosted-native-competitors.md), [limitations](docs/limitations.md).

## Limitations / Roadmap

**Current:** hosted CI establishes exactness/reproducibility, 1/2/4-thread engineering behavior, compressed-storage trade-offs, same-run native comparisons and pinned same-algorithm storage swaps for Teseo and Sortledton. The refined adaptive selector averages **1.66% overhead from regime-best** on the tested 36 root/regime configurations, but this is not evidence of universal selector optimality. The external-storage experiments isolate one BFS/storage interface on small synthetic graphs and are not general Teseo or Sortledton performance comparisons. Compression saves space but currently slows BFS traversal.

**Next:** execute the [unified canonical publication campaign](docs/canonical-publication-campaign.md) on the dedicated `velographx-benchmark` runner to cover checksum-pinned web/social/road graphs, the complete Kronecker/R-MAT series, the largest clean in-memory boundary, 1/2/4/8/16/32-thread scaling, NUMA placement and hardware counters. Broader same-machine dynamic-system comparisons, dedicated NVMe/`io_uring` throughput, weighted-dynamic evaluation and a frozen long-term Python API remain separate follow-ups. See [limitations](docs/limitations.md).

## License / Contributing

Apache License 2.0 — see [LICENSE](LICENSE). Contributions should be small, tested and benchmarked when performance claims change; see [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
