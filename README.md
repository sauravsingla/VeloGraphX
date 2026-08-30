# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for incremental analytics on continuously changing graphs.** It combines dynamic graph storage, localized algorithm repair, adaptive full recomputation, SIMD, multicore/NUMA-aware execution, compression, Python interoperability, and reproducible benchmark tooling.

The central research question is simple: **when should a changing graph be repaired incrementally, and when is full recomputation the better choice on modern CPUs?** VeloGraphX studies that decision together with the storage and execution architecture needed to make it practical. The project does not claim novelty from reimplementing individual graph algorithms.

## Research focus

VeloGraphX is organized around four related systems contributions:

1. **Adaptive incremental execution** — estimate affected work and choose between localized maintenance and full recomputation.
2. **Dynamic graph storage** — combine segmented CSR, packed sorted deltas, sparse row patches, reverse adjacency, and validated consolidation.
3. **CPU-aware execution** — integrate SIMD intersection kernels, multicore scheduling, push/pull frontiers, work stealing, and NUMA-aware policies.
4. **Reproducible evaluation** — pin datasets and competitor revisions, capture execution environments, enforce correctness gates, and retain benchmark artifacts.

Conceptually:

```text
update stream
    |
    v
dynamic graph storage
    |
    v
affected-work estimation
    |
    +---- localized repair
    |
    +---- full recomputation
    |
    v
CPU execution layer
    |
    v
maintained graph result
```

## Implemented systems

| Area | Implementation |
| --- | --- |
| Incremental analytics | Deletion-aware BFS / unweighted SSSP, weighted SSSP, exact triangle counting, connected components, k-core, localized PageRank repair |
| Dynamic storage | Segmented CSR, packed sorted deltas, sparse row patches, reverse adjacency, validated CSR consolidation |
| Adaptive execution | Affected-work estimation with incremental-vs-full fallback |
| CPU acceleration | Scalar, AVX2, AVX-512 and ARM NEON intersection paths |
| Multicore / NUMA | Push/pull frontiers, work stealing, topology discovery, affinity and locality-aware policies |
| Compression | Delta, variable-byte, blocked variable-byte and fixed-width adjacency coding |
| Python | pybind11 with NumPy, SciPy CSR and Apache Arrow ingestion |
| Out-of-core | Partition files, mmap/fallback reads, bounded cache, async loading and optional Linux `io_uring` prefetch |
| Reproducibility | Dataset checksums, immutable competitor pins, environment capture, correctness gates and retained artifacts |

## Evidence

The measurements below are **hosted-CI engineering results with correctness gates**. They demonstrate reproducible behavior on the stated workloads, but they should not be interpreted as universal or publication-grade performance claims.

### Exact dynamic triangle maintenance

Deterministic update batches on canonical SNAP graphs are maintained incrementally and checked against exact full recomputation.

| Dataset | Base graph | Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| `com-LiveJournal` | 34,681,189 edges | 0.01% / 3,469 | 4.066 ms | 13.126 s | **3,228.66x** |
| `com-LiveJournal` | 34,681,189 edges | 0.1% / 34,682 | 34.709 ms | 13.288 s | **382.85x** |
| `com-LiveJournal` | 34,681,189 edges | 1% / 346,812 | 415.377 ms | 14.119 s | **33.99x** |

The same campaign completed exact validation on **com-Orkut: 3,072,441 vertices, 117,185,083 edges, and 627,584,181 initial triangles**, establishing 100M-edge-class exact execution. See [CI-scale methodology and artifacts](docs/ci-scale-evidence.md).

### Same-run published exact reference

VeloGraphX was compared in the same process and on the same hosted runner with the pinned exact `GoldenCounter` reference distributed with public SIGMOD 2021 triangle-counting source code.

| Update batch | VeloGraphX | Published exact reference | Latency ratio |
| --- | ---: | ---: | ---: |
| 1% / 883 edges | 1.066 ms | 43.657 ms | **40.95x lower** |
| 5% / 4,412 edges | 6.782 ms | 47.095 ms | **6.94x lower** |
| 10% / 8,824 edges | 15.495 ms | 53.931 ms | **3.48x lower** |

All **15/15 measured results agreed exactly** across VeloGraphX incremental maintenance, the published exact reference, and VeloGraphX full recomputation. The comparison is specifically with the exact reference component at revision `1085ba049bb94451661d119284d7cd9b68687a81`; it is **not** a claim against the paper's approximate SWTC algorithm. See [full methodology](docs/same-run-published-baseline.md).

### Same-semantics dynamic BFS baseline

VeloGraphX was compared with the pinned SIGMOD 2021 **RisGraph** implementation (`4e77f774d4aa7cd0bf3011e713496573b70c91ab`) on checksum-pinned directed `web-Google`. The comparison uses the same source-order sliding-window stream, root, 99% initial import, 4,096-edge batches, and answer-ready timing envelope. Sparse SNAP vertex IDs are deterministically relabeled without reordering edges. The selected root reached **588,118 vertices**, avoiding a trivial BFS workload.

| System / policy | Mean answer-ready batch time |
| --- | ---: |
| VeloGraphX deletion-aware repair | **59.658 ms** |
| VeloGraphX legacy full-recompute policy | **118.039 ms** |
| RisGraph | **31.333 ms** |

Deletion-aware repair is **1.98x faster than VeloGraphX's former deletion policy** on this 13-batch workload, with **0/13 safety fallbacks** to full recomputation. **RisGraph remains approximately 1.90x faster** than the current VeloGraphX path, so the external performance gap is reduced but not closed. VeloGraphX matched an independent full BFS, RisGraph matched its own full rebuild, and the final BFS layer histogram agreed exactly across systems. See [external baseline methodology](docs/external-dynamic-baselines.md).

### Dynamic storage and steady-state maintenance

The current segmented-CSR / packed-delta / sparse-row-patch layout was compared with the pre-upgrade storage reconstructed from commit `22d05c6b54b9199c852062395b3d6536abca02d9`.

| Current / historical | 10M edges | 100M edges |
| --- | ---: | ---: |
| Loaded RSS | **0.967x** | **0.966x** |
| Mixed update throughput | **1.069x** | **1.628x** |
| Neighbor materialization latency | **0.780x** | **0.783x** |
| Explicit sparse compaction time | **1.657x** | **1.492x** |

The 10M case uses three repetitions. The 100M case is a single hosted execution and is treated as scale evidence rather than a low-noise performance result.

On a 60-epoch `com-Orkut` steady-state run, a scale-aware **1.50x** owned-storage cap for graphs with at least 100M directed arcs reduced consolidations from **15 to 6**, reduced consolidation time from **386.6 s to 156.1 s**, and increased maintenance-amortized throughput from **19.1k to 43.1k ops/s (2.25x)** relative to the conservative 1.25x policy. The trade-off was a larger owned-storage high-water mark (**1.529x vs 1.306x**) and approximately **6.6% higher peak RSS**. Both runs preserved exactly **234,370,166 directed arcs** and passed correctness checks.

For smaller graphs, the conservative **1.25x** storage cap remains the default. Full canonicalization is still an explicit validated O(E) operation; reducing that structural cost further remains an open systems optimization.

[Storage A/B](docs/storage-ab-evidence.md) · [row-patch accumulation](docs/row-patch-accumulation-evidence.md) · [100M+ canonicalization A/B](docs/canonicalization-ab-evidence.md) · [storage architecture](docs/dynamic-storage.md)

### CPU and adaptive execution

On a recorded 4-logical-CPU hosted runner, independent-query BFS throughput increased from **5,215.62 queries/s at 1 thread** to **11,105.1 queries/s at 2 threads (2.13x)** and **10,930.3 queries/s at 4 threads**, with identical result digests.

An adaptive-recomputation ablation places the incremental/full crossover around **20%** on the exercised Facebook workload. At a 50% insertion batch, forcing incremental execution was approximately **2.5x slower** than full recomputation. This identifies a decision boundary for the tested workload; it does not claim that the runtime policy always selects the oracle choice.

## Correctness and reproducibility

Exact-maintenance experiments are checked against trusted exact results or independent full recomputation. Iterative algorithms such as PageRank use convergence tolerances and L1/L∞ error against a separately converged full reference.

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, dynamic mutation correctness, forward/reverse storage consistency, optimized-vs-scalar kernels, scheduler/NUMA behavior, compression, native I/O, Python interoperability, dataset checksums, and benchmark artifact contracts. Deletion-aware BFS also has deterministic mixed-update differential testing against a fresh full BFS after every batch.

Methodology and boundaries: [benchmark methodology](docs/benchmark-methodology.md) · [published-baseline eligibility](docs/published-baseline-eligibility.md) · [external dynamic baselines](docs/external-dynamic-baselines.md) · [current limitations](docs/limitations.md)

## Research boundary

The current repository establishes reproducible hosted-CI execution, exact large-graph validation, a controlled same-run exact-reference comparison, a matched external dynamic-BFS comparison, 10M/100M storage measurements, repeated steady-state consolidation, and a 100M+-class canonicalization-policy A/B.

It does **not** establish universal superiority or full production maturity. Publication-grade conclusions still require dedicated hardware, repeated same-hardware external-system comparisons across multiple public graph families and seeds, 8/16/32+ physical-core scaling, true multi-socket NUMA experiments, hardware-counter analysis, broader update-locality studies, additional same-semantics dynamic-system comparisons, and independent reproduction.

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Python

```bash
cmake -S . -B build-python \
  -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_PYTHON=ON
cmake --build build-python -j
```

```python
import numpy as np
import velographx as vx

edges = np.array([[0, 1], [1, 2], [2, 0]], dtype=np.uint32)
graph = vx.from_numpy_edges(edges)
bfs = vx.IncrementalBFS(graph, 0)

update = vx.UpdateBatch()
update.add(2, 3)
bfs.apply(update)
print(bfs.distances)
```

## Contributing

Contributions are welcome across dynamic graph algorithms, storage, correctness, CPU performance, SIMD/NUMA portability, benchmarking, datasets, and interoperability. Performance-sensitive changes should include reproducible measurements and preserve the relevant correctness contract.

## License

Licensed under the [Apache License 2.0](LICENSE).
