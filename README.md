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
- Evaluates policy quality with pinned datasets, multiple roots/update regimes, repeated runs and retained artifacts; broader validation exposes regimes where the selector still loses.

See [related-work positioning](docs/related-work-positioning.md) and the [ablation study](docs/ablation-study.md).

## Results

GitHub-hosted results below are **engineering evidence, not publication-grade hardware claims**.

| Evidence | Result | Scope |
| --- | ---: | --- |
| Dynamic exactness stress | **2,000,000 updates, 0 BFS mismatches, 0 triangle mismatches** | 6 adversarial graph/update scenarios; check after every batch |
| Adaptive BFS, original campaign | **19/27 regime wins; 2.74% mean overhead from regime-best; 100% exact** | `ca-GrQc`, `soc-Epinions1`, `web-Google` |
| Adaptive BFS, broader campaign | **108/108 executions exact; 10.85% mean overhead** | 3 datasets × 4 roots × 3 regimes × 3 reps |
| Multicore BFS workload | **2.74× at 4 threads (~69% efficiency)** | 50K vertices, degree 8, 32 independent tasks, median of 5 reps |
| Connected-components workload | **2.50× at 4 threads (~62%)** | same workload contract |
| Triangle-count workload | **2.24× at 4 threads (~56%)** | same workload contract |
| Variable-byte adjacency compression | **3.25×–3.78× smaller** | 100K vertices, tested degrees 4/8/16 |
| Compressed BFS trade-off | **3.8×–5.7× slower than raw traversal** | same end-to-end compression campaign |
| Public dynamic graph scale | **875,713 vertices / 5,105,039 edges** | pinned `web-Google` normalization |

Evidence: Current Capacity Validation run `33467637208` (`9785416050`, `9785510370`, `9785516485`, `9785422293`) and adaptive run `33410705480` (`9767029881`).

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

So VeloGraphX wins the larger `web-Google` case but **loses on `ca-GrQc`**. NetworKit revision `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`; run `33301190847`, artifact `9766977170`.

RisGraph and other dynamic systems are relevant prior work, but results from different runners or incompatible semantics are intentionally **not merged into this table**.

## How it works

```text
update batch
   ↓
mutable graph: base CSR + deltas/row patches + consolidation
   ↓
affected-work / cost signals
   ↓
localized exact repair  ← selector →  full recomputation
   ↓
exact maintained result
```

Implemented analytics include BFS/unweighted SSSP, weighted SSSP, connected components, triangle count, k-core and PageRank-related paths. Runtime/storage code also includes SIMD intersections, work stealing, NUMA-aware policies, compression, partition caching and asynchronous partition loading.

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

The default build includes **28 CTest targets** plus benchmark executables.

## Reproduce

The headline campaigns are encoded as GitHub Actions workflows so dataset pins, versions, exactness gates and artifacts stay with the run.

```bash
# Requires an authenticated GitHub CLI.
gh workflow run current-capacity-validation.yml --ref main
gh workflow run hosted-native-competitors.yml --ref main

# Watch the latest dispatches.
gh run list --workflow current-capacity-validation.yml --limit 1
gh run list --workflow hosted-native-competitors.yml --limit 1
```

For a local correctness run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Benchmark contracts and interpretation rules: [benchmark methodology](docs/benchmark-methodology.md), [native competitors](docs/hosted-native-competitors.md), [limitations](docs/limitations.md).

## Limitations / Roadmap

**Current:** hosted CI establishes correctness, reproducibility, 1/2/4-thread engineering behavior, compressed-storage trade-offs and same-run native comparisons. The broader adaptive study is exact but reaches **25.8% mean overhead on `web-Google`**, showing root/workload sensitivity. Compression saves space but currently slows BFS traversal.

**Next:** dedicated 8/16/32+ core experiments; true multi-socket NUMA measurements; same-machine dynamic comparison with more external systems; dedicated NVMe/`io_uring` throughput; hardware counters; broader weighted-dynamic evaluation; selector features for reachability/component/work-density; a frozen long-term Python API. See [limitations](docs/limitations.md).

## License / Contributing

Apache License 2.0 — see [LICENSE](LICENSE). Contributions should be small, tested and benchmarked when performance claims change; see [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
