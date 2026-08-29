# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for incremental analytics on continuously changing graphs.** It repairs affected work when localized maintenance is appropriate and can fall back to full recomputation when the affected region becomes too large or the algorithm requires global work.

The engine combines segmented CSR storage, packed mutable deltas, explicit reverse adjacency, incremental graph algorithms, adaptive recomputation, SIMD kernels, multicore/NUMA-aware execution, compression, Python interoperability, and reproducible benchmark tooling.

## Credible evidence

The results below are reproducible **hosted-CI engineering measurements** with correctness gates. They are not presented as universal or publication-grade performance claims.

### Large-scale exact dynamic triangle maintenance

On canonical SNAP graphs, deterministic insertion batches are applied incrementally and then checked against exact full recomputation.

| Public dataset | Base graph | Update batch | Incremental | Full recomputation | Speedup | Exact |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `com-LiveJournal` | 34,681,189 edges | 0.01% / 3,469 | 4.066 ms | 13.126 s | **3,228.66x** | yes |
| `com-LiveJournal` | 34,681,189 edges | 0.1% / 34,682 | 34.709 ms | 13.288 s | **382.85x** | yes |
| `com-LiveJournal` | 34,681,189 edges | 1% / 346,812 | 415.377 ms | 14.119 s | **33.99x** | yes |

The same hosted campaign completed exact validation on **Orkut: 3,072,441 vertices / 117,185,083 edges / 627,584,181 initial triangles**, establishing 100M-edge-class exact execution. The retained large-scale artifact is reported without inventing a speedup that was not extracted from the run.

Methodology and artifacts: [hosted scale evidence](docs/ci-scale-evidence.md).

### Same-run published exact reference

VeloGraphX is also evaluated against the unmodified exact `GoldenCounter` reference distributed with public SIGMOD 2021 triangle-counting source code. Both implementations run in the same process, on the same hosted runner, with the same normalized graph and deterministic update batches.

| Update batch | VeloGraphX exact-answer-ready | Published exact reference | Latency ratio |
| --- | ---: | ---: | ---: |
| 1% / 883 edges | 1.066 ms | 43.657 ms | **40.95x lower** |
| 5% / 4,412 edges | 6.782 ms | 47.095 ms | **6.94x lower** |
| 10% / 8,824 edges | 15.495 ms | 53.931 ms | **3.48x lower** |

All **15/15 measured results agreed exactly** across VeloGraphX incremental maintenance, the published exact reference, and VeloGraphX full recomputation.

This comparison is specifically against the pinned exact reference component at revision `1085ba049bb94451661d119284d7cd9b68687a81`; it is not a claim against the paper's approximate SWTC algorithm. [Full methodology](docs/same-run-published-baseline.md).

### Dynamic-storage A/B evidence

The current segmented-CSR / packed-delta storage was compared with the actual pre-upgrade layout reconstructed from commit `22d05c6b54b9199c852062395b3d6536abca02d9` (`vector<vector<VertexId>>` plus per-vertex `unordered_set` insertion/deletion deltas). Both designs receive identical deterministic directed graphs and identical mixed updates.

| Metric | 10M edges | 100M edges |
| --- | ---: | ---: |
| Loaded RSS, current vs historical | 110.5 vs 114.2 MiB | 1,072.2 vs 1,109.7 MiB |
| RSS change | **3.3% lower** | **3.4% lower** |
| Mixed update throughput | **1.89x higher** | **1.33x higher** |
| Neighbor materialization latency | **21.6% lower** | **13.7% lower** |
| Bulk-load time | 2.86x slower | 3.09x slower |
| Full compaction time | **11.31x slower** | **16.16x slower** |
| Correctness gate | pass | pass |

The 10M case uses three repetitions; the 100M case is one hosted execution and is therefore scale evidence rather than a low-noise publication result.

For directed graphs, explicit reverse adjacency adds roughly the same compact CSR storage as the forward direction: about **42 MiB at 10M edges** and **420 MiB at 100M edges** in this degree-20 workload. That cost enables direct predecessor traversal instead of global predecessor discovery. The current global compaction path is the clearest remaining storage weakness and motivates segment-local or amortized compaction.

[Full storage A/B methodology and evidence](docs/storage-ab-evidence.md).

### CPU engineering evidence

On a recorded 4-logical-CPU hosted runner, independent-query BFS throughput increased from **5,215.62 queries/s at 1 thread** to **11,105.1 queries/s at 2 threads (2.13x)** and **10,930.3 queries/s at 4 threads**. The digest was identical across all thread counts, showing useful scaling to two threads and saturation on that hosted allocation.

The adaptive recomputation ablation places the incremental/full crossover around **20%** on the exercised Facebook workload; at a 50% insertion batch, forcing incremental work was about **2.5x slower** than full recomputation. This is an oracle decision-boundary experiment, not a claim that the runtime policy always achieves oracle selection.

## What is implemented

| Capability | Implementation |
| --- | --- |
| Incremental analytics | BFS / unweighted SSSP, weighted SSSP, exact triangle counting, connected components, k-core, localized iterative PageRank repair |
| Dynamic storage | Segmented CSR base, shared packed sorted delta arenas, insertion/deletion overlays, versioning and compaction |
| Reverse traversal | Explicit transposed CSR plus synchronized reverse deltas with `in_neighbors()` |
| PageRank validation | Localized repair with convergence controls, L1/L∞ comparison against a full converged reference, and correctness fallback mode |
| Adaptive execution | Affected-work estimation with incremental-vs-full fallback |
| CPU acceleration | Scalar, AVX2, AVX-512 and ARM NEON intersection paths |
| Multicore / NUMA | Push/pull frontiers, work stealing, topology discovery, affinity and locality-aware policies |
| Compression | Delta, variable-byte, blocked variable-byte and fixed-width adjacency coding |
| Python | pybind11 with NumPy, SciPy CSR and Apache Arrow ingestion |
| Out-of-core infrastructure | Partition files, mmap/fallback reads, bounded cache, async loading and optional Linux `io_uring` prefetch |
| Reproducibility | Dataset checksums, immutable competitor pins, environment capture, correctness gates and retained artifacts |

[Dynamic storage architecture](docs/dynamic-storage.md).

## Research contribution

VeloGraphX is not positioned as novel because it reimplements individual graph algorithms. Its research question is the **architecture-aware integration of incremental maintenance with adaptive recomputation on modern CPUs**: when should a changing graph be repaired locally, and when should the engine switch to full recomputation?

The experimental focus is therefore the **incremental-to-full crossover** as update density and affected work grow, together with storage layout, reverse dependency traversal, SIMD, multicore scheduling, NUMA placement and compression.

## Correctness and reproducibility

Performance evidence is reported only with an explicit validation contract. Exact-maintenance experiments compare against a trusted exact result or full recomputation. Iterative algorithms such as PageRank use convergence tolerances and L1/L∞ error against a separately converged full reference rather than claiming exact equality.

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, randomized dynamic mutations, storage forward/reverse consistency, destructive-update fallbacks, optimized-vs-scalar kernels, scheduler/NUMA behavior, compression, native I/O, Python interoperability, dataset checksums and publication-artifact contracts.

**Evidence:** [storage A/B](docs/storage-ab-evidence.md) · [same-run published reference](docs/same-run-published-baseline.md) · [hosted scale/CPU evidence](docs/ci-scale-evidence.md) · [published baseline eligibility](docs/published-baseline-eligibility.md) · [benchmark methodology](docs/benchmark-methodology.md) · [limitations](docs/limitations.md)

## Research boundary

Current results establish reproducible hosted-CI execution, exact large-graph validation, a controlled same-run published reference comparison, and measured 10M/100M storage behavior. They do **not** establish universal superiority or full production maturity.

Publication-grade conclusions still require controlled dedicated hardware, repeated 100M+ edge runs, 8/16/32+ core scaling, genuine multi-socket NUMA experiments, hardware counters, irregular real-graph storage campaigns, segment-local compaction experiments, broader same-semantics system comparisons, and independent reproduction.

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

Contributions are welcome across dynamic graph algorithms, storage, correctness, CPU performance, SIMD/NUMA portability, benchmarking, datasets and interoperability. Performance-sensitive changes should include reproducible measurements and preserve the relevant correctness contract.

High-value areas include **segment-local compaction, repeated controlled 100M+ storage campaigns, PageRank accuracy/runtime curves, 8/16/32+ core scaling, and multi-socket NUMA evaluation**.

## License

Apache-2.0.
