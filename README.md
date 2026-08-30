# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact analytics on changing graphs.** It combines compact mutable graph storage, localized exact repair, workload-aware incremental-vs-recomputation control, and reproducible benchmarking in one system.

## Research focus

VeloGraphX studies a practical systems question:

> **When a graph changes, when is it better to repair the current exact result incrementally, and when is full recomputation the better execution strategy?**

The project explores this question through a CPU-native architecture that couples:

- compact mutable graph storage,
- exact localized repair,
- graph-scale-conditioned online cost estimation,
- uncertainty-aware incremental/full selection,
- selector-owned recomputation control, and
- reproducible evaluation against full recomputation and external exact baselines.

The contribution is the **integration and evaluation of these mechanisms as one exact dynamic-graph execution architecture**. Individual graph algorithms such as BFS, SSSP, connected components, k-core, PageRank, and triangle counting are established techniques and are not presented as new algorithms. The detailed [related-work positioning](docs/related-work-positioning.md) states the boundary against prior incremental and streaming graph systems explicitly.

## Architecture

```text
update stream
    ↓
compact mutable graph storage
    ↓
work and cost estimation
    ↓
localized exact repair  ↔  full recomputation
    ↓
CPU execution
    ↓
exact maintained result
```

| Area | Implementation |
| --- | --- |
| Dynamic storage | Segmented CSR, packed deltas, sparse row patches, reverse adjacency, validated consolidation |
| Incremental analytics | BFS / unweighted SSSP, weighted SSSP, exact triangles, connected components, k-core, PageRank repair |
| Adaptive execution | Update-density preflight, affected-work signals, graph-scale conditioning, online cost estimates, uncertainty-aware selection |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20, pybind11, NumPy, SciPy CSR, Apache Arrow |
| Reproducibility | Checksum-pinned datasets, pinned baselines, exactness gates, environment capture, retained artifacts |

## Validated experimental evidence

Measurements below use reproducible benchmark contracts and independent exactness checks.

### Dynamic BFS vs NetworKit

A native C++ comparison with NetworKit 11.2.1 uses the same hosted runner, one thread, identical update streams, five paired repetitions, and independent full-BFS verification after every batch.

On `web-Google`:

| VeloGraphX | NetworKit | VX/NK | Difference |
| ---: | ---: | ---: | ---: |
| **31.512 ms** | 41.293 ms | **0.764x** | **23.7% lower latency** |

The canonical run completed with exact results in all paired repetitions. Multi-root validation also produced exact maintained BFS results across three deterministic `web-Google` roots and three `ca-GrQc` roots.

Canonical run: `33301190847`, artifact `9729078197`.

### Adaptive BFS policy

A development-suite evaluation across checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google` workloads compares the workload-aware selector with the best measured execution policy while including selector feature cost in adaptive timing.

| Metric | Result |
| --- | ---: |
| Exactness | **100%** |
| Mean oracle-relative regret | **3.15%** |
| p95 batch regret | **19.78%** |
| Worst-regime regret | **18.25%** |
| Mean selector decision cost | **7.49 µs** |

All pre-specified development acceptance criteria were satisfied: exactness = 100%, mean regret ≤ 5%, p95 batch regret ≤ 20%, and worst-regime regret ≤ 25%.

The evaluated selector combines graph-scale conditioning, online cost estimation, uncertainty-aware decisions, and selector-owned large-graph repair/recompute control. These figures are **development-suite adaptive-policy results** and are reported separately from external-baseline measurements. The [ablation study](docs/ablation-study.md) separates direct A/B evidence, historical mechanism evidence, and the publication-grade component-ablation contract.

### Incremental vs recomputation crossover

The benchmark suite directly compares four execution strategies:

- always incremental,
- always full recomputation,
- a fixed update threshold, and
- workload-aware adaptive execution.

A controlled campaign covered **3 checksum-pinned graph families × 3 roots × 3 update regimes × 5 repetitions**. All policy outputs passed independent full-BFS verification.

The measurements show clear crossover behavior: the most efficient strategy changes with graph structure, root state, and update intensity. The benchmark records per-batch latency, affected work, recomputation decisions, oracle-relative regret, and crossover points.

### Exact dynamic triangles

On `com-LiveJournal` with approximately **34.7 million base edges**:

| Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: |
| 0.01% | 4.066 ms | 13.126 s | **3,228.66x** |
| 0.1% | 34.709 ms | 13.288 s | **382.85x** |
| 1% | 415.377 ms | 14.119 s | **33.99x** |

Exact large-graph validation also completed on `com-Orkut` with **117,185,083 edges** and **627,584,181 initial triangles**.

Against the pinned exact `GoldenCounter` component from the public SIGMOD 2021 source, **15/15 measured results matched exactly**, with **3.48x–40.95x lower latency** on the evaluated workload.

## Correctness

VeloGraphX uses differential and independent-reference validation throughout its test and benchmark infrastructure. CI covers Ubuntu and macOS builds, Linux ASan/UBSan, dynamic graph mutation and storage consistency, incremental-vs-full differential correctness, SIMD/scalar agreement, scheduler and NUMA behavior, Python interoperability, dataset provenance, and benchmark contracts.

## Reproducibility

Experiments use checksum-pinned public datasets, immutable baseline revisions, explicit thread settings, exactness gates, environment capture, and retained GitHub Actions artifacts.

| Resource | Purpose |
| --- | --- |
| [Related-work positioning](docs/related-work-positioning.md) | Prior systems, overlap, and defensible contribution boundary |
| [Ablation study](docs/ablation-study.md) | Mechanism evidence and publication-grade ablation contract |
| [External dynamic baselines](docs/external-dynamic-baselines.md) | Competitor methodology and evidence |
| [CI-scale evidence](docs/ci-scale-evidence.md) | Reproducible CI measurements |
| [Published exact baseline](docs/same-run-published-baseline.md) | Exact published-reference comparison |
| [Storage evidence](docs/storage-ab-evidence.md) | Storage A/B experiments |
| [Benchmark methodology](docs/benchmark-methodology.md) | Measurement contract |
| [Current limitations](docs/limitations.md) | Detailed research scope |

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Enable Python bindings with `-DVELOGRAPHX_BUILD_PYTHON=ON`.

## License

Licensed under the [Apache License 2.0](LICENSE).
