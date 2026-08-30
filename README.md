# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for exact analytics on changing graphs.** Its systems focus is the interaction between compact mutable graph storage, localized incremental repair, and adaptive fallback to full recomputation.

> **Research question:** when should an update be repaired incrementally, and when is recomputation cheaper?

VeloGraphX does not claim novelty from BFS, SSSP, triangle counting, or the other individual graph algorithms. The research contribution under investigation is the **integrated CPU-native architecture and its workload-aware incremental-vs-recompute decision mechanism**.

## Architecture

```text
update stream
    ↓
compact mutable graph storage
    ↓
work estimation / adaptive decision
    ↓
localized exact repair  ↔  full recomputation
    ↓
CPU execution
    ↓
exact maintained result
```

| Component | Implementation |
| --- | --- |
| Dynamic storage | Segmented CSR, packed deltas, sparse row patches, reverse adjacency, validated consolidation |
| Incremental analytics | BFS / unweighted SSSP, weighted SSSP, exact triangles, connected components, k-core, PageRank repair |
| Adaptive execution | Update-density preflight, affected-work budgeting, incremental-vs-full fallback |
| CPU execution | SIMD intersections, multicore scheduling, push/pull frontiers, work stealing, NUMA-aware policies |
| Interoperability | C++20, pybind11, NumPy, SciPy CSR, Apache Arrow |
| Reproducibility | Checksum-pinned datasets, pinned competitors, exactness gates, environment capture, retained artifacts |

## Experimental evidence

The measurements below are **hosted-CI engineering results with independent exactness checks**. They are not claims of universal superiority.

### 1. Exact dynamic BFS vs NetworKit

NetworKit 11.2.1 is evaluated in native C++ on the same hosted runner with one thread, identical update streams, and independent full-BFS validation after every batch.

| Dataset | VeloGraphX | NetworKit | VX/NK | Result |
| --- | ---: | ---: | ---: | --- |
| `web-Google` | **31.512 ms** | 41.293 ms | **0.764x** | VeloGraphX lower latency |
| `ca-GrQc` | 0.1110 ms | **0.0808 ms** | **1.374x** | NetworKit lower latency |
| `soc-Epinions1` | 1.794 ms | **1.152 ms** | **1.582x** | NetworKit lower latency |

`soc-Epinions1` uses three deterministic reachability-screened roots and five paired repetitions per root; **15/15 pairs were exact**. This third graph family is included to expose both favorable and unfavorable performance regimes rather than only VeloGraphX wins.

Evidence: canonical two-dataset run `33301190847`, artifact `9729078197`; Epinions run `33303152827`, artifact `9729665685`.

### 2. Adaptive incremental-vs-recompute experiment

Dynamic BFS is evaluated under four policies:

| Policy | Decision |
| --- | --- |
| Always incremental | Repair every batch incrementally |
| Always full | Recompute BFS after every batch |
| Fixed threshold | Choose using a simple update-size threshold |
| VeloGraphX adaptive | Update-density preflight followed by an affected-work repair budget |

Validation spans **3 checksum-pinned graph families × 3 roots × 3 update regimes × 5 repetitions**. All measured outputs passed independent exact full-BFS verification.

| Metric | Adaptive result |
| --- | ---: |
| Regimes | 27 |
| Mean distance from fastest policy | **3.78%** |
| Regimes where adaptive was fastest | **10 / 27** |
| Predeclared worst-case acceptance bound | 1.30x |
| Observed worst case | **1.334x** |

The worst regime was `web-Google`, root `391806`, batch `6144`. Because **1.334x exceeded the predeclared 1.30x bound**, the candidate did **not** pass the promotion gate. This result is retained as evidence of measurable crossover behavior and of the remaining adaptive-selection problem, not as a claim that the selector is universally optimal.

Validation run `33312852351`; experimental commit `bb9f0162f20551f2da39212daa0e9c61cd9609bc`.

### 3. Exact dynamic triangles

On `com-LiveJournal` with 34.7M base edges:

| Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: |
| 0.01% | 4.066 ms | 13.126 s | **3,228.66x** |
| 0.1% | 34.709 ms | 13.288 s | **382.85x** |
| 1% | 415.377 ms | 14.119 s | **33.99x** |

Exact validation also completed on `com-Orkut` with **117,185,083 edges** and **627,584,181 initial triangles**.

Against the pinned exact `GoldenCounter` component from public SIGMOD 2021 source, **15/15 results matched exactly**, with **3.48x–40.95x lower latency** on the measured workload. The comparison is with the exact reference component, not the paper's approximate SWTC algorithm.

### 4. Additional competitor evidence

A separate native `web-Google` campaign measured **RisGraph at 31.333 ms** and **VeloGraphX at 59.658 ms**. Because this campaign ran on a different hosted runner from the NetworKit experiments, the measurements are deliberately **not combined into a three-system absolute ranking**.

## Correctness and reproducibility

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, dynamic mutation, incremental-vs-full differential correctness, storage consistency, SIMD/scalar agreement, scheduler/NUMA behavior, Python interoperability, dataset provenance, and benchmark contracts.

| Documentation | Purpose |
| --- | --- |
| [External dynamic baselines](docs/external-dynamic-baselines.md) | Competitor methodology and evidence |
| [CI-scale evidence](docs/ci-scale-evidence.md) | Reproducible CI measurements |
| [Published exact baseline](docs/same-run-published-baseline.md) | Exact published-reference comparison |
| [Storage evidence](docs/storage-ab-evidence.md) | Storage A/B experiments |
| [Benchmark methodology](docs/benchmark-methodology.md) | Measurement contract |
| [Current limitations](docs/limitations.md) | Scope and unresolved limitations |

## Research scope

**Supported by current evidence:** exact dynamic execution, compact mutable storage, localized repair, measurable incremental/recompute crossover behavior, and reproducible external comparisons.

**Not established:** universal performance superiority, a universally optimal adaptive policy, or production maturity. Stronger publication claims require dedicated hardware, broader graph and update distributions, same-machine competitor campaigns beyond NetworKit, multicore/NUMA evaluation, hardware counters, and independent reproduction.

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
