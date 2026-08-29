# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 engine for exact incremental graph analytics on continuously changing graphs.** Instead of rerunning an entire graph algorithm after every change, it repairs only the affected work when correctness permits and falls back to full recomputation when necessary.

Built for **dynamic graph analytics, temporal and streaming graphs, incremental BFS/SSSP, dynamic triangle counting, PageRank, graph compression, SIMD, multicore and NUMA-aware graph processing**.

> **Validated on public graphs:** exact incremental triangle maintenance is up to **84.77x faster than full recomputation** in the hosted-CI campaign, and on the same runner VeloGraphX reaches **40.95x lower exact-answer-ready latency** than a pinned published exact reference implementation at a 1% update batch.

## Results on public graphs

Every measurement below uses immutable dataset identity, deterministic normalization, five repetitions per update fraction, and exact incremental-vs-full validation.

| Public dataset | Graph family | Normalized graph | **1% updates** | **5% updates** | **10% updates** |
| --- | --- | ---: | ---: | ---: | ---: |
| `facebook-combined` | social | 4,039 vertices / 88,234 edges | **84.77x** | **17.90x** | **8.90x** |
| `ca-HepTh` | collaboration | 9,877 vertices / 25,973 edges | **59.05x** | **12.20x** | **6.59x** |
| `p2p-Gnutella08` | peer-to-peer | 6,301 vertices / 20,777 edges | **55.23x** | **11.53x** | **6.27x** |

Values are median `full recomputation / incremental` time. **Every result matched exact full recomputation.** The complete 13-fraction sweeps showed no median crossover through a changed-edge batch equal to 200% of the original base-edge count.

These are reproducible **hosted-CI engineering measurements**, not universal performance claims. Tiny update fractions are intentionally excluded from the headline results. [Full public-dataset methodology and evidence](docs/multi-dataset-crossover.md).

## Same-run published exact reference

To avoid misleading cross-paper comparisons, VeloGraphX also runs beside the **unmodified exact `GoldenCounter` reference distributed with public SIGMOD 2021 triangle-counting source code**. Both execute in the same process on the same GitHub-hosted runner with GCC 13.3.0, the identical normalized `facebook-combined` graph, identical deterministic insertion batches, and five repetitions.

| Update batch | VeloGraphX exact-answer-ready | Published exact reference | **Latency ratio** | VeloGraphX throughput |
| --- | ---: | ---: | ---: | ---: |
| **1% / 883 edges** | **1.066 ms** | 43.657 ms | **40.95x lower** | **0.828M updates/s** |
| **5% / 4,412 edges** | **6.782 ms** | 47.095 ms | **6.94x lower** | **0.651M updates/s** |
| **10% / 8,824 edges** | **15.495 ms** | 53.931 ms | **3.48x lower** | **0.569M updates/s** |

All **15/15 measured results agreed exactly** across VeloGraphX incremental maintenance, the published exact reference, and VeloGraphX full recomputation.

`GoldenCounter` accepts updates dynamically but computes the exact global count when queried, so the comparison uses **exact-answer-ready latency**: insertion plus exact query. This is specifically a comparison with the paper repository's exact reference component at pinned revision `1085ba049bb94451661d119284d7cd9b68687a81`—**not a claim against the SIGMOD 2021 approximate SWTC algorithm itself**. [Methodology, provenance and raw metric definitions](docs/same-run-published-baseline.md).

## Why VeloGraphX

| Capability | What is implemented |
| --- | --- |
| **Incremental analytics** | BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core and localized PageRank repair |
| **Dynamic graph storage** | Batched insert/delete deltas, versioning, compaction, weighted updates, temporal history and snapshots |
| **Adaptive execution** | Affected-work estimation with explainable incremental-vs-full fallback |
| **CPU acceleration** | Adaptive intersection plus AVX2, AVX-512, ARM NEON and scalar fallback |
| **Multicore + NUMA** | Push/pull frontiers, work stealing, adaptive scheduling, topology discovery, affinity and locality-aware placement |
| **Graph compression** | Delta, variable-byte, blocked variable-byte and SIMD-friendly fixed-width adjacency coding |
| **Python interoperability** | pybind11 with NumPy, SciPy CSR and Apache Arrow ingestion |
| **Out-of-core infrastructure** | Partition files, mmap/fallback reads, bounded cache, async loading and optional Linux `io_uring` prefetch |
| **Reproducible research** | Checksum-verified datasets, immutable competitor pins, environment capture, correctness gates and provenance-rich artifacts |

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
graph.apply(update)
bfs.apply(update)
print(bfs.distances)
```

## Correctness and reproducibility

Performance claims are gated by correctness. The repository tests static and dynamic algorithms, destructive-update fallbacks, randomized mutations, optimized-vs-scalar kernels, scheduler/NUMA behavior, compression, native I/O, Python interoperability, and ASan/UBSan builds.

Hosted CI also builds pinned SuiteSparse:GraphBLAS `v10.3.2`, LAGraph `v1.2.2`, and GAP Benchmark Suite `v1.5`. A normalized BFS gate requires identical input metadata and identical full-distance output across builtin, LAGraph and GAP implementations.

Benchmark infrastructure includes checksum-verified public datasets, deterministic update generation, environment capture, immutable competitor revisions, repeated measurements and validated result artifacts.

**Evidence:** [same-run published reference](docs/same-run-published-baseline.md) · [public multi-dataset results](docs/multi-dataset-crossover.md) · [benchmark methodology](docs/benchmark-methodology.md) · [native competitors](docs/hosted-native-competitors.md) · [limitations](docs/limitations.md)

## Research boundary

The current numbers are **hosted-CI engineering evidence**, not publication-grade claims of universal superiority. Strong conclusions about 100M+ edge workloads, 1/2/4/8/16/32+ thread scaling, genuine multi-socket NUMA behavior, hardware counters, and complete native-system comparisons require controlled dedicated hardware.

Keeping that boundary explicit is part of the project's reproducibility contract.

## Use cases

VeloGraphX targets graphs that change continuously and where repeated full recomputation is expensive: **social and communication networks, payment and fraud graphs, cybersecurity networks, infrastructure/dependency graphs, temporal event graphs, knowledge graphs, and dynamic graph-systems research**.

## Contributing

Contributions are welcome across dynamic graph algorithms, correctness, CPU performance, SIMD/NUMA portability, benchmarking, datasets, interoperability and reproducibility. Performance-sensitive changes should include reproducible measurements and retain correctness comparison against a trusted reference.

## License

Apache-2.0. See [`LICENSE`](LICENSE).

---

**Keywords:** dynamic graph algorithms · incremental graph analytics · dynamic graph processing · temporal graphs · streaming graphs · CPU graph analytics · C++20 graph library · graph systems · SIMD · AVX2 · AVX-512 · NUMA · graph compression · BFS · SSSP · PageRank · exact triangle counting · k-core · Python graph analytics · GraphBLAS · LAGraph · reproducible benchmarking
