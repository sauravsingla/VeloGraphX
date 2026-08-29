# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![Public Dataset Plan](https://github.com/sauravsingla/VeloGraphX/actions/workflows/public-dataset-plan.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/public-dataset-plan.yml)
[![Publication Artifact Contract](https://github.com/sauravsingla/VeloGraphX/actions/workflows/publication-artifact.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/publication-artifact.yml)
[![Hosted Native Competitors](https://github.com/sauravsingla/VeloGraphX/actions/workflows/hosted-native-competitors.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/hosted-native-competitors.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native research and engineering platform for incremental graph analytics on continuously changing graphs.** It combines dynamic graph algorithms, adaptive recomputation, SIMD-aware kernels, multicore scheduling, NUMA-aware execution policies, compression, Python interoperability, out-of-core infrastructure, and reproducible benchmarking in a modern C++20 codebase.

> **Guiding question:** when a large graph changes only slightly, how little work can a modern CPU perform while still producing the correct updated answer?

VeloGraphX is intended for researchers and systems engineers exploring **dynamic graph analytics**, **incremental graph processing**, **CPU graph engines**, **SIMD graph processing**, **NUMA-aware execution**, and **reproducible graph-system benchmarking**.

## Why VeloGraphX?

Many graph workloads repeatedly recompute an answer from scratch even when only a small part of the graph has changed. VeloGraphX explores a different execution model: identify affected work, repair only what is necessary when correctness permits, and fall back to full recomputation when localized repair is unsafe or estimated to be unhelpful.

The project is deliberately CPU-first. Its optimization order is:

1. **Avoid unnecessary graph work.**
2. **Improve memory locality.**
3. **Scale across CPU cores and NUMA domains.**
4. **Optimize instructions with SIMD and compression-aware kernels.**

Correctness comes before speed: incremental results are checked against full recomputation, optimized kernels are compared with scalar references, and benchmark claims are explicitly separated from CI fixtures and unmeasured research hypotheses.

## Highlights

The table below describes **implemented engineering capability**. It should not be read as a claim that every capability has already been validated at publication scale.

| Area | Implemented capability |
| --- | --- |
| Dynamic graph storage | Versioned graph storage with batched insert/delete deltas, threshold compaction, weighted updates, temporal history and snapshots |
| Incremental algorithms | BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core and localized PageRank repair |
| Adaptive execution | Affected-work estimates and explainable incremental-vs-full-recompute selection |
| Graph kernels | Scalar, galloping, bitmap and architecture-specific AVX2 / AVX-512 / ARM NEON paths with runtime ISA detection and scalar fallback |
| Multicore execution | Sparse/dense frontiers, push/pull selection, work stealing, adaptive grain sizing and degree/frontier-aware scheduling |
| NUMA | Linux topology discovery, affinity, first-touch, `mbind`, partition placement, local queues and local-first stealing observability, with portable fallback |
| Compression | Delta, variable-byte, blocked variable-byte and SIMD-friendly fixed-width adjacency coding with adaptive codec recommendation |
| Python | Optional pybind11 bindings with NumPy, SciPy CSR and Apache Arrow ingestion plus dynamic/incremental APIs |
| Out-of-core | Partition-file backend, mmap/fallback reads, async loading, bounded cache, readahead and optional Linux `io_uring` prefetch |
| Reproducibility | Checksum-verified datasets, competitor adapters, version-pin readiness contracts, environment capture, campaign orchestration and publication-artifact validation |

## Algorithms

VeloGraphX currently includes static or dynamic/incremental support for:

- Breadth-first search (BFS) and unweighted shortest paths
- Weighted single-source shortest paths (SSSP)
- Connected components
- PageRank
- Triangle counting
- k-core decomposition
- Common-neighbor and Jaccard-style neighborhood operations
- Adaptive neighbor intersection

The incremental engine uses localized repair where correctness conditions permit and explicit full-recompute fallback where a destructive update cannot be safely or efficiently repaired.

## Quick start

### Build the C++20 core

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Builds are continuously checked on Ubuntu and macOS, with additional ASan/UBSan coverage on Linux.

### Enable Python bindings

Python bindings require `pybind11`; NumPy, SciPy and Apache Arrow interoperability is supported.

```bash
cmake -S . -B build-python \
  -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_PYTHON=ON
cmake --build build-python -j
```

Example Python usage:

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

At a high level, VeloGraphX combines five layers:

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

This separation is intentional: algorithmic work reduction, memory locality, parallel scheduling and instruction-level optimization can be evaluated independently through ablations.

## Testing and correctness

VeloGraphX maintains broad native and interoperability coverage, including:

- static and dynamic graph correctness tests;
- incremental BFS, connected-components, k-core, PageRank, triangle and weighted-SSSP tests;
- deletion and destructive-update fallback cases;
- randomized dynamic mutation campaigns;
- scalar-versus-optimized kernel differential checks;
- NUMA policy, partitioning and scheduler tests;
- work-stealing and concurrency stress coverage;
- compression and codec-policy tests;
- native I/O, partition-cache and async partition-loader tests;
- Python NumPy, SciPy CSR and Apache Arrow interoperability checks;
- Linux ASan/UBSan runs.

CI-scale tests establish engineering correctness and portability for the exercised configurations. They do not substitute for research-scale hardware evaluation.

## Benchmarking and reproducibility

VeloGraphX includes infrastructure for reproducible experiments rather than embedding unverified headline numbers in the README.

The repository contains:

- checksum-verified dataset preparation;
- public-dataset manifests and readiness contracts;
- competitor adapters for builtin reference, NetworkX, igraph, NetworKit and rustworkx;
- normalized external/native runner contracts for SuiteSparse:GraphBLAS/LAGraph and GAP;
- immutable competitor version-pin readiness checks;
- benchmark-environment capture for OS, CPU, compiler and competitor identities;
- update-fraction, thread-scaling, NUMA, hardware-counter and ablation campaign definitions;
- codec throughput/compression-ratio tooling and codec-policy calibration;
- provenance-rich publication artifact generation and integrity validation.

A hosted-CI engineering campaign exercises core benchmarks, update-fraction measurements, paired ablations, conservative thread checks and normalized BFS adapters. Its result bundle is provenance-captured and integrity-validated and remains explicitly marked `research_claim: false`.

### Hosted-CI engineering snapshot

A fresh CI-scale evidence campaign on commit `3c894df099b8cf21ad0b017087f3a02779d0914b` completed successfully on a GitHub-hosted Ubuntu x86_64 runner with 4 logical CPUs and GCC 13.3.0. The benchmark preflight and provenance-rich result-bundle validators both passed. These numbers are **fixture/synthetic-workload engineering measurements only**; they are not publication-grade performance claims and should not be generalized across machines or datasets.

| Exercised benchmark | Hosted-CI result |
| --- | --- |
| Incremental triangle count, 20k vertices / 60k base edges, 1% updates | **25.0x** median full-recompute / incremental time over 5 runs |
| Same triangle workload, 5% updates | **5.72x** median |
| Same triangle workload, 10% updates | **3.07x** median |
| Adaptive neighbor intersection | Faster than scalar on **5 of 6** exercised synthetic size pairs; scalar remained faster for the smallest 8x8 case |
| Fixed-width vectorized decode vs fixed-width scalar decode | **2.82x dense**, **4.40x medium**, **3.25x sparse** on the exercised 100k-value synthetic families |
| BFS adapter correctness | builtin, NetworkX 3.6.1, igraph 1.0.0, NetworKit 11.2.1 and rustworkx 0.18.1 produced the same normalized result digest on the identical fixture |
| 1-vs-2-thread smoke check | Completed successfully; **no scaling claim** is made from the hosted runner |

Very small update fractions are intentionally omitted from the README because microsecond-scale timing resolution can inflate ratios. Full raw results, environment metadata, dataset checksum, repetitions and validation artifacts are retained by the CI-scale evidence workflow.

Hosted CI builds immutable pinned native stacks for SuiteSparse:GraphBLAS `v10.3.2`, LAGraph `v1.2.2` and GAP Benchmark Suite `v1.5` on the same runner. In addition to small native engineering runs, the normalized BFS correctness gate now requires builtin, LAGraph and GAP to use the same dataset, source and directedness and to produce the same full-distance-vector digest. This establishes a strict same-input/full-output correctness comparison for the exercised BFS fixture. It remains explicitly non-publication evidence: `research_claim: false`, `publication_grade: false` and `publication_ready: false`.

The repository also contains public-dataset incremental crossover engineering evidence, including ca-GrQc work, and ongoing work to extend that evidence across additional graph families. These measurements remain engineering evidence unless and until they are repeated under controlled, publication-grade experimental conditions.

Hosted CI validates contracts, correctness and small-scale engineering behavior, **not publication-grade superiority or large-scale scalability**. Dedicated-hardware benchmark results should only be treated as project claims when dataset identity, environment, version pins, methodology and result artifacts are captured by the documented workflow.

Useful references:

- [`docs/ci-scale-evidence.md`](docs/ci-scale-evidence.md) — hosted-CI engineering evidence and caveats
- [`docs/hosted-native-competitors.md`](docs/hosted-native-competitors.md) — pinned GraphBLAS/LAGraph/GAP hosted build and engineering evidence
- [`docs/benchmark-methodology.md`](docs/benchmark-methodology.md) — experimental methodology
- [`docs/prompt-coverage.md`](docs/prompt-coverage.md) — authoritative implemented / partial / unmeasured status matrix
- [`docs/limitations.md`](docs/limitations.md) — known implementation and validation limitations
- [`docs/native-competitors.md`](docs/native-competitors.md) — LAGraph/GraphBLAS and GAP execution contract
- [`benchmarks/competitor-research-plan.json`](benchmarks/competitor-research-plan.json) — competitor version-pin readiness plan

## What is validated today

The repository has engineering evidence for correctness, cross-framework adapter normalization, incremental update behavior, optimized intersection paths, compression/decompression behavior, Python interoperability, reproducibility contracts and small hosted-CI execution campaigns.

Hosted CI additionally verifies that pinned GraphBLAS/LAGraph and GAP native stacks build on the same runner. For the exercised normalized BFS fixture, builtin, LAGraph and GAP are checked against identical dataset/source/directedness metadata and an identical full-distance-vector digest. This is credible cross-engine correctness evidence for that fixture, not a publication-grade performance comparison.

Public-dataset incremental crossover work is also underway and already includes ca-GrQc evidence. This is useful evidence for development and hypothesis testing, but it is intentionally not presented as a universal performance result.

## What is not claimed yet

VeloGraphX does **not** currently claim publication-grade superiority over other graph engines.

The following remain unmeasured at publication grade or materially environment-dependent:

- true multi-socket NUMA locality, bandwidth and remote-traffic behavior;
- large 1/2/4/8/16/32+ thread-scaling studies on dedicated hardware;
- 100M+ edge research-scale experiments;
- broad multi-dataset crossover characterization across graph families;
- complete like-for-like competitor performance comparisons on identical dedicated hardware, including native LAGraph/GraphBLAS and GAP;
- publication-grade hardware-counter and ablation measurements;
- research-scale codec throughput/compression-ratio campaigns;
- calibrated production codec thresholds from representative public-dataset measurements;
- research-scale NVMe / `io_uring` throughput and overlap studies.

The engineering infrastructure for many of these experiments already exists. The remaining gap is controlled execution and evidence; results should not be inferred before those experiments are completed.

## Current limitations

Some capabilities are intentionally conservative or environment-dependent:

- localized incremental algorithms may fall back to full recomputation for destructive or difficult updates;
- architecture-specific SIMD paths retain scalar fallback and require representative hardware calibration before broad performance conclusions;
- NUMA execution policies are implemented on Linux, but genuine multi-socket benefits require suitable hardware to measure;
- out-of-core infrastructure supports mmap, async loading, bounded caching, readahead and optional `io_uring`, but research-scale NVMe behavior has not yet been established;
- adaptive execution and codec-selection thresholds require broader public-dataset calibration;
- normalized builtin/LAGraph/GAP BFS full-distance correctness is established for the hosted fixture, while broader datasets and dedicated-hardware publication-grade competitor performance evidence remain outstanding.

For status decisions, [`docs/prompt-coverage.md`](docs/prompt-coverage.md) is the authoritative capability matrix.

## When VeloGraphX may be useful

VeloGraphX is a good fit for exploring workloads such as continuously changing social, communication, transaction, infrastructure, cybersecurity or knowledge graphs where updates are frequent and repeated full recomputation can be expensive.

It is also intended as a research platform for questions around dynamic graph algorithms, incremental computation, CPU graph analytics, SIMD graph kernels, NUMA scheduling, graph compression, temporal graphs, out-of-core graph processing and reproducible graph-system benchmarking.

## Repository map

```text
include/        C++ public headers and core engine components
src/            implementation sources
python/         pybind11 bindings
benchmarks/     benchmark programs, fixtures and research plans
tools/          dataset, competitor, calibration, provenance and experiment tooling
datasets/       dataset manifests and reproducibility fixtures
docs/           architecture, methodology, limitations and research notes
.github/        CI and reproducibility contract workflows
```

## Project status

VeloGraphX is under active development. The authoritative status is maintained in [`docs/prompt-coverage.md`](docs/prompt-coverage.md), which distinguishes implemented functionality from partial, environment-dependent, unmeasured and future work.

If you are evaluating the repository for research or systems work, start with this README, then review the benchmark methodology, hosted-CI evidence note and prompt-coverage matrix before interpreting benchmark output.

## Contributing

Contributions that improve correctness, dynamic graph algorithms, CPU performance, SIMD/NUMA portability, dataset reproducibility, benchmarking discipline, documentation or interoperability are welcome. Performance-sensitive changes should include reproducible measurements where practical, and optimized execution paths should retain correctness coverage or comparison against a trusted reference.

## License

Apache-2.0. See [`LICENSE`](LICENSE).

---

**Search terms:** dynamic graph algorithms · incremental graph analytics · incremental graph processing · CPU graph analytics · C++ graph library · SIMD graph algorithms · AVX2 · AVX-512 · ARM NEON · NUMA graph processing · temporal graphs · out-of-core graph analytics · graph compression · PageRank · BFS · SSSP · connected components · triangle counting · k-core · Python graph analytics · GraphBLAS benchmarking
