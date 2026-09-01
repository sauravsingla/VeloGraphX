# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact analytics on changing graphs.** It combines mutable graph storage, localized exact repair, and workload-aware incremental-vs-full recomputation.

## Why VeloGraphX?

The individual graph algorithms are established. VeloGraphX focuses on their **system-level integration for dynamic graphs**: compact mutable storage, exact localized repair, affected-work and cost signals, and adaptive selection between incremental repair and full recomputation.

```text
updates → mutable graph → work/cost estimation
        → localized exact repair  ↔  full recomputation
        → exact maintained result
```

The runtime can choose recomputation before excessive repair work is incurred when an update batch crosses the incremental crossover region.

## Key results

Retained GitHub Actions measurements from 31 August–1 September 2026. Results are workload-specific hosted engineering evidence, not publication-grade hardware claims.

### Static BFS / SSSP vs GAP and LAGraph

| Algorithm | Threads | VeloGraphX | GAP | LAGraph |
| --- | ---: | ---: | ---: | ---: |
| BFS | 1 | **0.425 ms** | 0.790 ms | 4.000 ms |
| BFS | 2 | **0.420 ms** | 0.670 ms | 4.000 ms |
| BFS | 4 | **0.406 ms** | 0.830 ms | 4.800 ms |
| SSSP | 1 | 8.982 ms | **1.060 ms** | 23.500 ms |
| SSSP | 2 | 8.587 ms | **1.190 ms** | 23.600 ms |
| SSSP | 4 | 9.003 ms | **1.290 ms** | 27.100 ms |

Five repetitions per configuration on the same hosted runner with exactness checks. VeloGraphX led BFS on this workload; GAP led SSSP. Dynamic BFS showed the intended crossover: incremental repair won at **0.1% and 1%** updates, while full recomputation won at **5%**. Run `33418520303`, artifact `9768499895`.

### Adaptive dynamic BFS

On checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google` across 27 root/update-regime combinations:

| Metric | Result |
| --- | ---: |
| Exactness | **100%** |
| Adaptive regime wins | **19 / 27** |
| Mean overhead from regime-best | **~2.74%** |

Run `33410705480`, artifact `9767029881`.

### Architecture campaign

| Capability | Hosted result |
| --- | --- |
| Multicore BFS | **2.85×** speedup at 4 threads; ~71% efficiency |
| Compression | Variable-byte encoding reached **up to 4×** space reduction |
| SIMD decode | Fixed-width vectorized decoding was roughly **3–4× faster** than variable-byte decoding on representative tested distributions |
| Out-of-core paths | Partition cache, partition file, and async loader passed **5/5** runs each |
| NUMA/runtime | Detection, placement, partitioning, scheduling, work stealing, and execution-plan tests passed |

All thread-count runs produced identical result digests. The hosted runner exposed one NUMA node, so NUMA evidence is functional rather than cross-socket performance evidence. Run `33464276799`, artifact `9784295720`.

### Dynamic BFS vs NetworKit 11.2.1

On `web-Google`, one thread and identical update streams, five paired repetitions measured **36.328 ms for VeloGraphX vs 45.740 ms for NetworKit**, with independent full-BFS verification. Run `33301190847`, artifact `9766977170`.

## Architecture and capabilities

- **Exact analytics:** BFS / unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, PageRank
- **Dynamic execution:** localized exact repair with adaptive repair-vs-recompute selection
- **Mutable storage:** segmented CSR, packed deltas, sparse row patches, reverse adjacency, consolidation
- **Storage independence:** templated `BasicIncremental*<Graph>` implementations
- **Parallel execution:** SIMD intersections, push/pull frontiers, multicore scheduling, work stealing, NUMA-aware policies
- **Large-graph infrastructure:** compression, bounded partition cache, partition-file access, asynchronous loading
- **Interoperability:** C++20, pybind11, NumPy, SciPy CSR, Apache Arrow

## Correctness and reproducibility

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, storage consistency, incremental-vs-full differential tests, SIMD/scalar agreement, Python interoperability, dataset provenance, benchmark contracts, and independent reference checks.

Experiments use pinned datasets or baseline revisions, repeated measurements, exactness gates, environment capture, and retained artifacts. Dedicated controlled hardware is still required for publication-grade many-core scaling, cross-NUMA performance, storage-device throughput, and hardware-counter claims.

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Documentation

[Benchmark methodology](docs/benchmark-methodology.md) · [Hosted baselines](docs/hosted-native-competitors.md) · [Graph abstraction](docs/graph-abstraction.md) · [Ablation study](docs/ablation-study.md) · [Related work](docs/related-work-positioning.md) · [Limitations](docs/limitations.md)

## License

Licensed under the [Apache License 2.0](LICENSE).
