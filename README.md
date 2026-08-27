# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![Public Dataset Plan](https://github.com/sauravsingla/VeloGraphX/actions/workflows/public-dataset-plan.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/public-dataset-plan.yml)
[![Publication Artifact Contract](https://github.com/sauravsingla/VeloGraphX/actions/workflows/publication-artifact.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/publication-artifact.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native incremental graph analytics engine for large, continuously changing graphs.** It combines dynamic graph algorithms, adaptive recomputation, SIMD-aware kernels, multicore scheduling, NUMA-aware execution, compression, Python interoperability, and reproducible benchmarking in a modern C++20 codebase.

> **Guiding question:** when a massive graph changes only slightly, how little work can a modern CPU perform while still producing the correct updated answer?

VeloGraphX is designed for researchers and systems engineers working on **dynamic graph analytics**, **incremental graph processing**, **CPU graph engines**, **graph algorithm acceleration**, **SIMD graph processing**, **NUMA-aware graph systems**, and **large-scale graph benchmarking**.

## Why VeloGraphX?

Many graph workloads repeatedly recompute an answer from scratch even when only a small part of the graph changed. VeloGraphX explores a different execution model: identify the affected work, repair only what is necessary when that is safe, and fall back to full recomputation when it is not.

The project is deliberately CPU-first. Its optimization order is:

1. **Avoid unnecessary graph work.**
2. **Improve memory locality.**
3. **Scale across CPU cores and NUMA domains.**
4. **Optimize instructions with SIMD and compression-aware kernels.**

Correctness comes before speed: incremental results are checked against full recomputation, optimized kernels are compared with scalar references, and benchmark claims are separated from CI fixtures and unmeasured research hypotheses.

## Highlights

| Area | Implemented capability |
| --- | --- |
| Dynamic graph storage | Versioned graph storage with batched insert/delete deltas, threshold compaction, weighted updates, temporal history and snapshots |
| Incremental algorithms | BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core and localized PageRank repair |
| Adaptive execution | Affected-work estimates and explainable incremental-vs-full-recompute selection |
| Graph kernels | Scalar, galloping, bitmap, AVX2, AVX-512 and ARM NEON neighbor intersection with runtime ISA detection |
| Multicore execution | Sparse/dense frontiers, push/pull selection, work stealing, adaptive grain sizing and degree/frontier-aware scheduling |
| NUMA | Linux topology discovery, affinity, first-touch, `mbind`, partition placement, local queues and local-first stealing observability |
| Compression | Delta, variable-byte, blocked variable-byte and SIMD-friendly fixed-width adjacency coding with adaptive codec selection |
| Python | Optional pybind11 bindings with NumPy, SciPy CSR and Apache Arrow ingestion plus dynamic/incremental APIs |
| Out-of-core | Partition-file backend, mmap/fallback reads, async loading, bounded cache, readahead and optional `io_uring` prefetch |
| Reproducibility | Checksum-verified datasets, competitor adapters, version-pin readiness contracts, environment capture, campaign orchestration and publication artifact validation |

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

The incremental engine uses localized repair where correctness conditions permit and explicit full-recompute fallback where a destructive update cannot be repaired safely.

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

## Benchmarking and reproducibility

VeloGraphX includes infrastructure for reproducible experiments rather than embedding unverified headline numbers in the README.

The repository contains:

- checksum-verified dataset preparation;
- public-dataset readiness contracts;
- competitor adapters for builtin reference, NetworkX, igraph, NetworKit and rustworkx;
- normalized external/native runner contracts for SuiteSparse:GraphBLAS/LAGraph and GAP;
- immutable competitor version-pin readiness checks;
- benchmark-environment capture for OS, CPU, compiler and competitor identities;
- update-fraction, thread-scaling, NUMA, hardware-counter and ablation campaign definitions;
- codec throughput/compression-ratio tooling and codec-policy calibration;
- provenance-rich publication artifact generation and integrity validation.

Hosted CI runs validate contracts and correctness, **not publication-grade performance**. Dedicated-hardware benchmark results should only be treated as project claims when the corresponding dataset, environment, version pins and result artifacts are captured by the documented workflow.

Useful references:

- [`docs/benchmark-methodology.md`](docs/benchmark-methodology.md) — experimental methodology
- [`docs/prompt-coverage.md`](docs/prompt-coverage.md) — implemented vs partial vs unmeasured work
- [`docs/limitations.md`](docs/limitations.md) — known limitations
- [`docs/native-competitors.md`](docs/native-competitors.md) — LAGraph/GraphBLAS and GAP execution contract
- [`benchmarks/competitor-research-plan.json`](benchmarks/competitor-research-plan.json) — competitor version-pin readiness plan

## What is not claimed yet

VeloGraphX does **not** currently claim publication-grade superiority over other graph engines. Several evaluation milestones require dedicated hardware and completed public-dataset runs.

Still unmeasured or environment-dependent are true multi-socket NUMA locality and remote-traffic behavior, large 1/2/4/8/16/32+ thread-scaling studies, 100M+ edge experiments, the full update-fraction crossover campaign, complete competitor comparisons on identical hardware, publication-grade hardware-counter and ablation measurements, research-scale codec campaigns, and NVMe/`io_uring` throughput studies.

The engineering infrastructure for many of these experiments is already present; the remaining gap is execution and evidence, not permission to infer results.

## When VeloGraphX may be useful

VeloGraphX is a good fit for exploring workloads such as continuously changing social, communication, transaction, infrastructure, cybersecurity or knowledge graphs where graph updates are frequent and repeated full recomputation is expensive.

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

If you are evaluating the repository for research or systems work, start with the README, then review the benchmark methodology and prompt-coverage matrix before interpreting any benchmark output.

## Contributing

Contributions that improve correctness, dynamic graph algorithms, CPU performance, SIMD/NUMA portability, dataset reproducibility, benchmarking discipline, documentation or interoperability are welcome. Please keep performance claims reproducible and accompany optimized paths with correctness coverage or reference comparisons where appropriate.

## License

Apache-2.0. See [`LICENSE`](LICENSE).

---

**Search terms:** dynamic graph algorithms · incremental graph analytics · incremental graph processing · CPU graph analytics · C++ graph library · SIMD graph algorithms · AVX2 · AVX-512 · ARM NEON · NUMA graph processing · temporal graphs · out-of-core graph analytics · graph compression · PageRank · BFS · SSSP · connected components · triangle counting · k-core · Python graph analytics · GraphBLAS benchmarking
