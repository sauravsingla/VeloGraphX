# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![Public Dataset Plan](https://github.com/sauravsingla/VeloGraphX/actions/workflows/public-dataset-plan.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/public-dataset-plan.yml)
[![Publication Artifact Contract](https://github.com/sauravsingla/VeloGraphX/actions/workflows/publication-artifact.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/publication-artifact.yml)
[![Hosted Native Competitors](https://github.com/sauravsingla/VeloGraphX/actions/workflows/hosted-native-competitors.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/hosted-native-competitors.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 graph analytics engine for dynamic and continuously changing graphs.** It focuses on **incremental graph processing**: updating results after small graph changes instead of recomputing everything from scratch whenever correctness permits.

It combines **dynamic graph algorithms, adaptive recomputation, SIMD kernels, multicore scheduling, NUMA-aware execution, graph compression, Python interoperability, out-of-core infrastructure, and reproducible benchmarking** in one research and engineering platform.

> **Core question:** when only a small part of a large graph changes, how little work can a modern CPU perform while still producing the correct updated answer?

## Why VeloGraphX matters

Traditional graph analytics often reruns an algorithm over the entire graph after every update. VeloGraphX explores a different execution model: identify affected work, repair only what is necessary, and fall back to full recomputation when incremental repair is unsafe or not worthwhile.

This makes the project relevant to **dynamic graph analytics, temporal graphs, streaming graph workloads, CPU graph processing, incremental BFS/SSSP, dynamic triangle counting, graph compression, SIMD graph kernels, NUMA graph processing, and reproducible graph systems research**.

## Public graph evidence

A fresh GitHub-hosted campaign completed successfully across three structurally different public graph families. Every run used immutable source identity, deterministic normalization, five repetitions per update fraction, exact incremental-vs-full triangle-count validation, environment capture, preflight validation, and provenance-rich result bundles.

| Public dataset | Graph family | Normalized graph | **1% updates** | **5% updates** | **10% updates** |
| --- | --- | ---: | ---: | ---: | ---: |
| `facebook-combined` | social | 4,039 vertices / 88,234 edges | **84.77x** | **17.90x** | **8.90x** |
| `ca-HepTh` | collaboration | 9,877 vertices / 25,973 edges | **59.05x** | **12.20x** | **6.59x** |
| `p2p-Gnutella08` | peer-to-peer network | 6,301 vertices / 20,777 edges | **55.23x** | **11.53x** | **6.27x** |

The values are median `full recomputation / incremental` time over five repetitions. **Every measurement in all three complete 13-fraction sweeps matched full recomputation exactly.** No median crossover was observed within the configured sweep through a changed-edge batch equal to 200% of the original base-edge count.

These are **hosted-CI engineering measurements, not publication-grade universal performance claims**. The headline table intentionally excludes tiny update fractions where extremely short incremental timings can exaggerate ratios. See [`docs/multi-dataset-crossover.md`](docs/multi-dataset-crossover.md) for the full methodology, caveats, and evidence contract.

### Same-run published exact reference comparison

A separate hosted-CI experiment runs VeloGraphX beside the **unmodified exact `GoldenCounter` reference implementation distributed with the public SIGMOD 2021 triangle-counting source**. Both execute in the same comparison process with GCC 13.3.0, the identical normalized `facebook-combined` graph, identical deterministic insertion batches, and five repetitions per update fraction. Every VeloGraphX incremental result, GoldenCounter exact result, and VeloGraphX full recomputation agreed exactly.

`GoldenCounter` dynamically accepts the updates but computes the exact global count when `triangle_count()` is queried, so its fair **exact-answer-ready** latency is insertion time plus exact-query time. VeloGraphX maintains the exact answer during its incremental update.

| Update batch | VeloGraphX exact-answer-ready | Published `GoldenCounter` exact-answer-ready | Answer-ready latency ratio | VeloGraphX throughput | VeloGraphX vs own full recompute |
| --- | ---: | ---: | ---: | ---: | ---: |
| **1% / 883 edges** | **1.066 ms** | 43.657 ms | **40.95x lower** | **0.828 M updates/s** | **82.45x** |
| **5% / 4,412 edges** | **6.782 ms** | 47.095 ms | **6.94x lower** | **0.651 M updates/s** | **18.05x** |
| **10% / 8,824 edges** | **15.495 ms** | 53.931 ms | **3.48x lower** | **0.569 M updates/s** | **9.18x** |

This is **not** a claim that VeloGraphX outperforms the SIGMOD 2021 SWTC algorithm itself: SWTC is an approximate sliding-window algorithm with different semantics. The comparison is specifically against the paper repository's pinned exact reference component, revision `1085ba049bb94451661d119284d7cd9b68687a81`. Full provenance, source hashes, update-only/query timings, methodology and interpretation boundaries are in [`docs/same-run-published-baseline.md`](docs/same-run-published-baseline.md).

### Peer-reviewed research context

Dynamic triangle counting is an established research problem rather than a repository-specific benchmark. **Makkar, Bader and Green, HiPC 2017, _Exact and Parallel Triangle Counting in Dynamic Graphs_** studied exact batched triangle maintenance under graph insertions/deletions and emphasized avoiding recomputation from scratch by processing affected structure. Their published implementation targets GPU hardware and reports up to 32M analytic updates/s, or up to 11M updates/s when graph-structure maintenance is included. [Paper](https://doi.org/10.1109/HiPC.2017.00011) · [Author copy](https://davidbader.net/publication/2017-mbg/)

**De Stefani et al., KDD 2016, _TRIÈST_** studied fully dynamic insertion/deletion streams with fixed memory, using reservoir sampling to maintain high-quality **approximate** triangle counts. This is a related but different operating point from VeloGraphX, whose campaign above maintains an **exact** triangle count and validates every incremental result against full recomputation. [KDD paper](https://doi.org/10.1145/2939672.2939771) · [KDD overview](https://www.kdd.org/kdd2016/subtopic/view/triest-counting-local-and-global-triangles-in-fully-dynamic-streams-with-fi)

The older cross-paper numbers remain methodological context rather than a speed ranking because those systems use different hardware, graph scales, update models, memory constraints, and experimental setups. VeloGraphX's public-dataset incremental/full figures above are speedups over its own exact full recomputation; the same-run `GoldenCounter` table is kept separate because it satisfies the stronger same-run normalization contract.

## Hosted-CI engineering results

A separate CI-scale evidence campaign completed successfully on a GitHub-hosted Ubuntu x86_64 runner with 4 logical CPUs and GCC 13.3.0. Benchmark preflight, correctness checks, provenance capture and the result-bundle validator all passed.

| Exercised benchmark | Validated hosted-CI result |
| --- | --- |
| Incremental triangle count, 20k vertices / 60k base edges, **1% updates** | **25.0x** median full-recompute / incremental time over 5 runs |
| Same workload, **5% updates** | **5.72x** median |
| Same workload, **10% updates** | **3.07x** median |
| Adaptive neighbor intersection | Faster than scalar on **5 of 6** exercised synthetic size pairs |
| Fixed-width vectorized decode | **2.82x dense**, **4.40x medium**, **3.25x sparse** vs scalar decode |
| BFS adapter correctness | builtin, NetworkX, igraph, NetworKit and rustworkx produced the **same normalized result digest** on the identical fixture |
| Native BFS correctness gate | builtin, pinned LAGraph and pinned GAP use the same dataset/source/directedness and must produce the **same full-distance-vector digest** |
| 1-vs-2-thread smoke check | Completed successfully; **no scaling claim** is made from hosted CI |

These are **small-scale engineering measurements**, not publication-grade claims of universal superiority. Very small update fractions are intentionally omitted because microsecond-scale timer resolution can inflate ratios.

## What is implemented

| Area | Capability |
| --- | --- |
| Dynamic graph storage | Versioned graph storage, batched insert/delete deltas, compaction, weighted updates, temporal history and snapshots |
| Incremental algorithms | BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core and localized PageRank repair |
| Adaptive execution | Affected-work estimation and explainable incremental-vs-full-recompute selection |
| SIMD graph kernels | Scalar, galloping, bitmap, AVX2, AVX-512 and ARM NEON paths with runtime ISA detection and scalar fallback |
| Multicore execution | Sparse/dense frontiers, push/pull selection, work stealing, adaptive grain sizing and degree/frontier-aware scheduling |
| NUMA | Linux topology discovery, affinity, first-touch, `mbind`, partition placement, local queues and local-first stealing observability |
| Graph compression | Delta, variable-byte, blocked variable-byte and SIMD-friendly fixed-width adjacency coding |
| Python | Optional pybind11 bindings with NumPy, SciPy CSR and Apache Arrow ingestion |
| Out-of-core | Partition-file backend, mmap/fallback reads, async loading, bounded cache, readahead and optional Linux `io_uring` prefetch |
| Reproducibility | Checksum-verified datasets, competitor adapters, environment capture, version-pin contracts and provenance-rich result artifacts |

## Algorithms

VeloGraphX includes static or dynamic/incremental support for:

- Breadth-first search (BFS) and unweighted shortest paths
- Weighted single-source shortest paths (SSSP)
- Connected components
- PageRank
- Triangle counting
- k-core decomposition
- Common-neighbor and Jaccard-style neighborhood operations
- Adaptive neighbor intersection

Incremental paths use localized repair where correctness conditions permit and explicit full-recompute fallback for destructive or difficult updates.

## Quick start

### Build the C++20 core

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Builds are continuously checked on Ubuntu and macOS, with additional Linux ASan/UBSan coverage.

### Python bindings

```bash
cmake -S . -B build-python \
  -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_PYTHON=ON
cmake --build build-python -j
```

Example:

```python
import numpy as np
import velographx as vx

edges = np.array([[0, 1], [1, 2], [2, 0]], dtype=np.uint32)
graph = vx.from_numpy_edges(edges)

bfs = vx.IncrementalBFS(graph, 0)

update = vx.UpdateBatch()
update.add(2, 3)
graph.apply(update)
bfs.apply(update)

print(bfs.distances)
```

## Architecture

```text
Input / interoperability
  edge lists | native binary | NumPy | SciPy CSR | Arrow
                         |
                         v
Versioned dynamic graph storage
  base adjacency + insertion/deletion deltas + compaction
                         |
                         v
Incremental / adaptive algorithms
  affected-work estimation + localized repair + full fallback
                         |
                         v
CPU execution engine
  frontier policies + work stealing + NUMA placement + SIMD kernels
                         |
                         v
Research / reproducibility layer
  datasets + campaigns + competitors + environment + result artifacts
```

The layers are separated so algorithmic work reduction, memory locality, parallel scheduling and instruction-level optimization can be evaluated independently through ablations.

## Correctness and testing

Correctness comes before speed. VeloGraphX maintains coverage for:

- static and dynamic graph correctness;
- incremental BFS, connected components, k-core, PageRank, triangle counting and weighted SSSP;
- deletion and destructive-update fallbacks;
- randomized mutation campaigns;
- scalar-versus-optimized kernel differential checks;
- NUMA policy, partitioning and scheduler behavior;
- work-stealing and concurrency stress;
- compression and codec policy;
- native I/O, partition cache and asynchronous loading;
- Python NumPy, SciPy CSR and Arrow interoperability;
- Linux ASan/UBSan runs.

Hosted CI also builds immutable pinned native stacks for SuiteSparse:GraphBLAS `v10.3.2`, LAGraph `v1.2.2` and GAP Benchmark Suite `v1.5`. The normalized native BFS gate requires identical input metadata and identical full-distance output across builtin, LAGraph and GAP for the exercised fixture.

## Reproducible benchmarking

The repository includes infrastructure for repeatable graph-systems experiments:

- checksum-verified dataset preparation;
- public-dataset manifests and readiness contracts;
- adapters for builtin, NetworkX, igraph, NetworKit and rustworkx;
- native runner contracts for SuiteSparse:GraphBLAS/LAGraph and GAP;
- immutable competitor version-pin readiness checks;
- benchmark-environment capture;
- update-fraction, thread-scaling, NUMA, hardware-counter and ablation campaigns;
- codec throughput/compression-ratio tooling;
- provenance-rich publication artifact generation and integrity validation.

Useful references:

- [`docs/same-run-published-baseline.md`](docs/same-run-published-baseline.md) — same-run exact published-reference comparison and provenance
- [`docs/multi-dataset-crossover.md`](docs/multi-dataset-crossover.md) — validated public multi-dataset incremental evidence
- [`docs/ci-scale-evidence.md`](docs/ci-scale-evidence.md) — hosted-CI evidence and caveats
- [`docs/hosted-native-competitors.md`](docs/hosted-native-competitors.md) — pinned GraphBLAS/LAGraph/GAP hosted evidence
- [`docs/benchmark-methodology.md`](docs/benchmark-methodology.md) — experimental methodology
- [`docs/prompt-coverage.md`](docs/prompt-coverage.md) — implemented / partial / unmeasured capability matrix
- [`docs/limitations.md`](docs/limitations.md) — known limitations
- [`docs/native-competitors.md`](docs/native-competitors.md) — native competitor execution contract

## Research boundary

VeloGraphX does **not** currently claim publication-grade superiority over other graph engines. Dedicated hardware and larger controlled campaigns are still required for strong claims about:

- multi-socket NUMA locality and remote-memory behavior;
- 1/2/4/8/16/32+ thread scaling;
- 100M+ edge workloads;
- broader/larger public-dataset behavior under controlled hardware;
- complete like-for-like native competitor performance;
- hardware-counter conclusions;
- research-scale codec throughput;
- NVMe / `io_uring` out-of-core performance.

This boundary is intentional: benchmark claims should follow captured dataset identity, environment, version pins, methodology and validated result artifacts.

## Where VeloGraphX may be useful

VeloGraphX is aimed at workloads where graphs change continuously and repeated full recomputation can become expensive, including:

- social and communication graphs;
- payment, transaction and fraud graphs;
- cybersecurity and network graphs;
- infrastructure and dependency graphs;
- temporal and event-driven graphs;
- knowledge graphs;
- research on dynamic graph algorithms and CPU graph systems.

## Repository map

```text
include/        C++ public headers and core engine components
src/            implementation sources
python/         pybind11 bindings
benchmarks/     benchmark programs, fixtures and research plans
tools/          dataset, competitor, calibration, provenance and experiment tooling
datasets/       dataset manifests and reproducibility fixtures
docs/           architecture, methodology, limitations and research notes
.github/        CI and reproducibility workflows
```

## Contributing

Contributions are welcome across correctness, dynamic graph algorithms, CPU performance, SIMD/NUMA portability, datasets, reproducibility, benchmarking, documentation and interoperability. Performance-sensitive changes should include reproducible measurements where practical, and optimized paths should retain correctness comparison against a trusted reference.

## License

Apache-2.0. See [`LICENSE`](LICENSE).

---

**Topics:** dynamic graph algorithms · incremental graph analytics · dynamic graph processing · temporal graphs · CPU graph analytics · C++ graph library · graph systems · SIMD graph processing · AVX2 · AVX-512 · NUMA · graph compression · BFS · SSSP · PageRank · triangle counting · k-core · Python graph analytics · GraphBLAS · LAGraph · GAP Benchmark Suite · reproducible benchmarking