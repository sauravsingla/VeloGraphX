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

Results below are retained GitHub Actions measurements from 31 August–1 September 2026. They are workload-specific engineering evidence, not publication-grade hardware claims.

### GAP + LAGraph comparison

| Algorithm | Threads | VeloGraphX | GAP | LAGraph |
| --- | ---: | ---: | ---: | ---: |
| BFS | 1 | **0.425 ms** | 0.790 ms | 4.000 ms |
| BFS | 2 | **0.420 ms** | 0.670 ms | 4.000 ms |
| BFS | 4 | **0.406 ms** | 0.830 ms | 4.800 ms |
| SSSP | 1 | 8.982 ms | **1.060 ms** | 23.500 ms |
| SSSP | 2 | 8.587 ms | **1.190 ms** | 23.600 ms |
| SSSP | 4 | 9.003 ms | **1.290 ms** | 27.100 ms |

Five repetitions, same hosted runner and source, exactness checks enabled. Dynamic BFS crossed from incremental repair winning at **0.1% and 1%** updates to full recomputation winning at **5%**. Run `33418520303`, artifact `9768499895`.

### Adaptive BFS policy

Across checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google` (3 roots × 3 update regimes × 5 repetitions), exactness was **100%**, the adaptive policy won **19/27** regimes, and mean overhead from the measured regime-best path was **~2.74%**. Run `33410705480`, artifact `9767029881`.

### Hosted architecture campaign

A 1 September 2026 campaign exercised multicore scaling, compression, bounded-cache/partition-file loading, and NUMA-aware runtime paths on a 4-logical-CPU hosted Linux runner.

| Capability | Result |
| --- | --- |
| BFS multicore scaling | **2.85× speedup at 4 threads**, ~71% efficiency; 2 threads reached ~2.04× |
| Compression | Variable-byte reached **up to 4× space reduction** on tested adjacency distributions |
| Decode throughput | SIMD-friendly fixed-width decoding was roughly **3–4× faster** than compact variable-byte decoding on representative tested distributions |
| Out-of-core paths | Partition cache, partition file, and async loader passed **5/5 runs each** |
| NUMA/runtime | Detection, policy, partitioner, scheduler, work stealing, frontier scheduling, and execution-plan tests all passed |

Thread-count results produced identical digests. The runner exposed one NUMA node, so NUMA results are functional validation rather than cross-socket performance evidence. Run `33464276799`, artifact `9784295720`.

### Dynamic BFS vs NetworKit 11.2.1

On `web-Google`, one thread, identical updates and five paired repetitions, VeloGraphX measured **36.328 ms** versus **45.740 ms** for NetworKit, with independent full-BFS verification. Run `33301190847`, artifact `9766977170`.

## Capabilities

- Exact BFS / unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank
- Segmented CSR, packed deltas, sparse row patches, reverse adjacency, and validated consolidation
- Storage-independent `BasicIncremental*<Graph>` algorithm implementations
- Adaptive exact execution using update density, affected-work signals, graph scale, online cost estimates, and uncertainty-aware selection
- SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, and NUMA-aware policies
- Compression, bounded partition caching, partition-file access, and asynchronous loading
- C++20, pybind11, NumPy, SciPy CSR, and Apache Arrow interoperability

## Correctness and reproducibility

CI covers Ubuntu/macOS builds, Linux ASan/UBSan, storage consistency, incremental-vs-full differential testing, SIMD/scalar agreement, Python interoperability, dataset provenance, benchmark contracts, and independent reference checks.

Experiments use pinned datasets or baseline revisions, repeated measurements, exactness gates, environment capture, and retained artifacts. Hosted CI is engineering evidence; dedicated controlled hardware is still required for publication-grade many-core scaling, cross-NUMA performance, storage-device throughput, and hardware-counter claims.

## Documentation

[Benchmark methodology](docs/benchmark-methodology.md) · [Hosted baselines](docs/hosted-native-competitors.md) · [Graph abstraction](docs/graph-abstraction.md) · [Ablation study](docs/ablation-study.md) · [Related work](docs/related-work-positioning.md) · [Limitations](docs/limitations.md)

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## License

Licensed under the [Apache License 2.0](LICENSE).
