# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for incremental analytics on continuously changing graphs.** Instead of rerunning an entire graph algorithm after every change, it identifies and repairs affected work when the algorithm and update pattern permit, and can fall back to full recomputation when localized maintenance is no longer advantageous or appropriate.

The project combines **dynamic graph storage, incremental BFS/SSSP, exact dynamic triangle counting, connected components, k-core, localized PageRank repair, graph compression, SIMD kernels, multicore execution, NUMA-aware policies, adaptive recomputation, and reproducible benchmarking**.

For algorithms with exact incremental maintenance in the exercised workloads, results are validated against full recomputation. Iterative algorithms such as PageRank use localized, tolerance- and iteration-controlled repair and should be interpreted separately from exact-maintenance results.

> **Large-scale exact validation:** on the 34.68M-edge LiveJournal graph, exact incremental triangle maintenance matched full recomputation and was **33.99x faster at a 1% insertion batch**. The same hosted-CI campaign also completed exact validation on the **117.19M-edge Orkut graph**, demonstrating 100M-edge-class execution. On the same hosted runner, VeloGraphX also reaches **40.95x lower exact-answer-ready latency** than a pinned published exact reference on `facebook-combined` at a 1% update batch.

## Large-scale exact results

The hosted scale campaign uses canonical SNAP graphs, deterministic missing-edge insertion batches, incremental maintenance followed by exact full recomputation, and an equality gate on every exercised batch. These are **hosted-CI engineering measurements**, not universal or publication-grade performance claims.

| Public dataset | Base graph | Update batch | Incremental | Full recomputation | **Speedup** | Exact |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `com-LiveJournal` | **34,681,189 edges** | 0.01% / 3,469 | **4.066 ms** | 13.126 s | **3,228.66x** | yes |
| `com-LiveJournal` | **34,681,189 edges** | 0.1% / 34,682 | **34.709 ms** | 13.288 s | **382.85x** | yes |
| `com-LiveJournal` | **34,681,189 edges** | 1% / 346,812 | **415.377 ms** | 14.119 s | **33.99x** | yes |

LiveJournal contains 3,997,962 unique SNAP vertices; the raw identifier space used by the benchmark extends to 4,036,538 because the source IDs are not densely remapped. The initial published triangle count is 177,820,130, and every exercised post-update count matched exact recomputation.

The same workflow also completed the canonical **Orkut exact-scale job** on **3,072,441 vertices / 117,185,083 edges / 627,584,181 initial triangles**. Its workflow artifact is retained as `velographx-orkut-exact-scale`. This README deliberately does not invent a speedup from the large artifact: the completed exact 100M-edge-class run is reported separately from the fully extracted LiveJournal timings.

Workflow evidence: `Hosted Scale CPU Evidence`, run `33249261192`, commit `15a13c425fa736d0062acfde7d44dd07a19b95b0`. The run produced separate retained artifacts for LiveJournal exact scale, Orkut exact scale, and hosted CPU ablation/scaling.

## Results on smaller public graphs

The earlier repeated public-dataset campaign used immutable dataset identity, deterministic normalization, five repetitions per update fraction, and exact incremental-vs-full validation.

| Public dataset | Graph family | Normalized graph | **1% updates** | **5% updates** | **10% updates** |
| --- | --- | ---: | ---: | ---: | ---: |
| `facebook-combined` | social | 4,039 vertices / 88,234 edges | **84.77x** | **17.90x** | **8.90x** |
| `ca-HepTh` | collaboration | 9,877 vertices / 25,973 edges | **59.05x** | **12.20x** | **6.59x** |
| `p2p-Gnutella08` | peer-to-peer | 6,301 vertices / 20,777 edges | **55.23x** | **11.53x** | **6.27x** |

Values are median `full recomputation / incremental` time and every result matched exact full recomputation. These measurements predate the later full-recompute optimization used by the scale/ablation campaign, so they are retained as reproducible historical campaign results rather than mixed with the newer crossover measurements. [Full public-dataset methodology and evidence](docs/multi-dataset-crossover.md).

## Same-run published exact reference

To avoid misleading cross-paper comparisons, VeloGraphX also runs beside the **unmodified exact `GoldenCounter` reference distributed with public SIGMOD 2021 triangle-counting source code**. Both execute in the same process on the same GitHub-hosted runner with GCC 13.3.0, the identical normalized `facebook-combined` graph, identical deterministic insertion batches, and five repetitions.

| Update batch | VeloGraphX exact-answer-ready | Published exact reference | **Latency ratio** | VeloGraphX throughput |
| --- | ---: | ---: | ---: | ---: |
| **1% / 883 edges** | **1.066 ms** | 43.657 ms | **40.95x lower** | **0.828M updates/s** |
| **5% / 4,412 edges** | **6.782 ms** | 47.095 ms | **6.94x lower** | **0.651M updates/s** |
| **10% / 8,824 edges** | **15.495 ms** | 53.931 ms | **3.48x lower** | **0.569M updates/s** |

All **15/15 measured results agreed exactly** across VeloGraphX incremental maintenance, the published exact reference, and VeloGraphX full recomputation.

`GoldenCounter` accepts updates dynamically but computes the exact global count when queried, so the comparison uses **exact-answer-ready latency**: insertion plus exact query. This is specifically a comparison with the paper repository's exact reference component at pinned revision `1085ba049bb94451661d119284d7cd9b68687a81`—**not a claim against the SIGMOD 2021 approximate SWTC algorithm itself**. [Methodology, provenance and raw metric definitions](docs/same-run-published-baseline.md).

## Recent published-baseline screen

VeloGraphX applies a strict rule before placing another paper in a direct same-CPU performance table: the original implementation must be public, runnable on the same CPU environment, consume the same normalized graph and deterministic update stream, and produce matching exact semantics.

A post-2023 literature screen found strong related work, including **SRDS 2024 DTC**, **SIGMOD 2025 temporal triangle counting**, **TACO 2025 Cheetah**, and **IEEE TPDS 2026 EDTC**, but none currently satisfies every condition for a scientifically valid same-CPU exact dynamic-triangle comparison. DTC is approximate/distributed, the SIGMOD 2025 work targets temporal-window queries, Cheetah has no reproduced public implementation in the current screen, and EDTC is GPU-based.

The repository therefore **does not manufacture cross-paper speedups from incompatible hardware or semantics**. [Published baseline eligibility matrix](docs/published-baseline-eligibility.md).

## Why VeloGraphX

| Capability | What is implemented |
| --- | --- |
| **Incremental analytics** | BFS / unweighted SSSP, weighted SSSP, exact triangle counting, connected components, k-core and localized iterative PageRank repair |
| **Dynamic graph storage** | Batched insert/delete deltas, versioning, compaction, weighted updates, temporal history and snapshots |
| **Adaptive execution** | Affected-work estimation with explainable incremental-vs-full fallback |
| **CPU acceleration** | Adaptive intersection plus AVX2, AVX-512, ARM NEON and scalar fallback |
| **Multicore + NUMA** | Push/pull frontiers, work stealing, adaptive scheduling, topology discovery, affinity and locality-aware placement |
| **Graph compression** | Delta, variable-byte, blocked variable-byte and SIMD-friendly fixed-width adjacency coding |
| **Python interoperability** | pybind11 with NumPy, SciPy CSR and Apache Arrow ingestion |
| **Out-of-core infrastructure** | Partition files, mmap/fallback reads, bounded cache, async loading and optional Linux `io_uring` prefetch |
| **Reproducible research** | Checksum-verified datasets, immutable competitor pins, environment capture, correctness gates and provenance-rich artifacts |

## Research contribution

VeloGraphX is not positioned as novel because it implements individual graph algorithms such as BFS, PageRank or triangle counting. Those algorithms are well established. The research question is the **architecture-aware integration of incremental maintenance with adaptive recomputation on modern CPUs**.

The project studies when a changing graph should be repaired locally and when the affected region has grown enough that a full recomputation is preferable. It combines that decision with graph topology, update density, dynamic storage, compression, SIMD-friendly kernels, multicore scheduling and NUMA-aware execution.

A central experimental objective is therefore the **incremental-to-full crossover**: how the cost of localized maintenance changes as the update fraction and affected region grow, and whether an execution policy can select the lower-cost strategy without compromising the required result semantics.

## Implementation boundary

VeloGraphX currently contains both scale-oriented systems components and simpler reference-oriented graph abstractions. The core dynamic adjacency representation uses a compact base plus insertion/deletion deltas and periodic compaction; several specialized paths add compression, partitioning, SIMD, NUMA and out-of-core behavior around that foundation.

The current implementation should therefore be read as a **research engine under active systems development**, not as a claim that every code path is already a production-optimized billion-edge representation. In particular, some localized algorithms may still perform globally expensive dependency discovery in their reference implementation. Improving reverse/in-neighbor access, compact dynamic adjacency, allocator behavior and cache-efficient update structures remains part of the scale roadmap.

For PageRank specifically, the localized repair implementation is iterative: it propagates material residual changes through an affected region, uses a configurable tolerance and local iteration budget, and can fall back to full recomputation when the repair region becomes large. PageRank results should therefore be described in terms of controlled iterative convergence/validation rather than the exact-maintenance language used for exact dynamic triangle counting.

## CPU engineering evidence

The hosted scale campaign also exercises systems components behind the engine. On the recorded 4-logical-CPU runner, independent-query BFS throughput increased from **5,215.62 queries/s at 1 thread** to **11,105.1 queries/s at 2 threads** (**2.13x**), with **10,930.3 queries/s at 4 threads**, showing saturation on the hosted 4-vCPU environment. The digest was identical across 1/2/4 threads.

The adaptive intersection path materially improves medium/large and skewed adjacency intersections (up to roughly **5.15x** versus scalar in the exercised cases), while tiny intersections can favor scalar execution. Variable-byte coding achieved **2–4x** size reduction on the generated codec families; vectorized fixed-width decoding traded compression ratio for roughly **2.4–3.8x** faster decode than its scalar counterpart.

The adaptive recomputation ablation places the incremental/full crossover around **20%** on the exercised Facebook workload after the newer full-recompute optimization; at a 50% insertion batch, forcing incremental work was about **2.5x slower** than full recomputation. This is an **oracle decision-boundary ablation**, not a claim that the current production policy achieves oracle selection.

These are engineering checks, not publication-grade performance claims. [Hosted CI evidence campaign](docs/ci-scale-evidence.md).

## Quick start

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Ubuntu and macOS builds are continuously tested, with Linux ASan/UBSan coverage.

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

# Incremental algorithm apply() also applies the batch to its graph.
bfs.apply(update)
print(bfs.distances)
```

## Correctness and reproducibility

Performance claims are gated by result validation. The repository tests static and dynamic algorithms, destructive-update fallbacks, randomized mutations, optimized-vs-scalar kernels, scheduler/NUMA behavior, compression, native I/O, Python interoperability, and ASan/UBSan builds.

For **exact-maintenance experiments**, incremental output is compared with a trusted exact result or full recomputation and a performance number is reported only when the equality gate passes. Dynamic triangle-counting results shown in this README follow that rule.

For **iterative/localized algorithms** such as PageRank, exact equality is not implied merely by the word “incremental.” Validation must instead specify convergence tolerance, iteration limits and comparison methodology. This distinction is intentional so that exact-maintenance evidence is not generalized to algorithms with iterative semantics.

Hosted CI also builds pinned SuiteSparse:GraphBLAS `v10.3.2`, LAGraph `v1.2.2`, and GAP Benchmark Suite `v1.5`. A normalized BFS gate requires identical input metadata and identical full-distance output across builtin, LAGraph and GAP implementations.

Benchmark infrastructure includes checksum-verified public datasets, deterministic update generation, environment capture, immutable competitor revisions, repeated measurements and validated result artifacts.

**Evidence:** [same-run published reference](docs/same-run-published-baseline.md) · [published baseline eligibility](docs/published-baseline-eligibility.md) · [public multi-dataset results](docs/multi-dataset-crossover.md) · [hosted CPU evidence](docs/ci-scale-evidence.md) · [benchmark methodology](docs/benchmark-methodology.md) · [native competitors](docs/hosted-native-competitors.md) · [limitations](docs/limitations.md)

## Research boundary

The current numbers are **hosted-CI engineering evidence**, not publication-grade claims of universal superiority. The repository includes exact hosted-CI execution on a **117.19M-edge Orkut graph**, exact timed results on LiveJournal, and measured 1/2/4-thread independent-query scaling. These results establish reproducibility and meaningful scale evidence for the exercised paths; they do not establish that every VeloGraphX subsystem has equivalent scaling characteristics.

Publication-grade conclusions still require controlled dedicated hardware, repeated large-graph campaigns, 8/16/32+ core scaling, genuine multi-socket NUMA experiments, hardware counters, memory-footprint characterization, broader same-semantics native-system comparisons, and independent reproduction.

The strongest future evidence will come from reproducible **crossover curves** across datasets and update fractions, large-graph memory measurements, dedicated NUMA scaling, and external validation through independent users, contributors or peer review.

Keeping that boundary explicit is part of the project's reproducibility contract.

## Use cases

VeloGraphX targets graphs that change continuously and where repeated full recomputation is expensive: **social and communication networks, payment and fraud graphs, cybersecurity networks, infrastructure/dependency graphs, temporal event graphs, knowledge graphs, and dynamic graph-systems research**.

## Contributing

Contributions are welcome across dynamic graph algorithms, correctness, CPU performance, SIMD/NUMA portability, benchmarking, datasets, interoperability and reproducibility. Performance-sensitive changes should include reproducible measurements and retain correctness comparison against a trusted reference where exact semantics are expected.

High-value areas include compact dynamic adjacency, efficient reverse/in-neighbor indexing, cache-conscious update structures, allocator/memory-footprint optimization, multi-socket NUMA experiments, and independent benchmark reproduction.

## License

Apache-2.0. See [`LICENSE`](LICENSE).

---

**Keywords:** dynamic graph algorithms · incremental graph analytics · dynamic graph processing · temporal graphs · streaming graphs · CPU graph analytics · C++20 graph library · graph systems · SIMD · AVX2 · AVX-512 · NUMA · graph compression · BFS · SSSP · PageRank · exact triangle counting · k-core · Python graph analytics · GraphBLAS · LAGraph · reproducible benchmarking
