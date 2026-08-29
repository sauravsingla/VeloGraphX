# VeloGraphX

[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)

**VeloGraphX is a CPU-native C++20 research engine for incremental analytics on continuously changing graphs.** It combines dynamic graph storage, localized algorithm repair, adaptive full recomputation, SIMD, multicore/NUMA-aware execution, compression, Python interoperability, and reproducible benchmark tooling.

## Credible evidence

The results below are reproducible **hosted-CI engineering measurements with correctness gates**. They are not presented as universal or publication-grade performance claims.

### Exact dynamic triangle maintenance

On canonical SNAP graphs, deterministic update batches are maintained incrementally and verified against exact full recomputation.

| Dataset | Base graph | Update batch | Incremental | Full recomputation | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| `com-LiveJournal` | 34,681,189 edges | 0.01% / 3,469 | 4.066 ms | 13.126 s | **3,228.66x** |
| `com-LiveJournal` | 34,681,189 edges | 0.1% / 34,682 | 34.709 ms | 13.288 s | **382.85x** |
| `com-LiveJournal` | 34,681,189 edges | 1% / 346,812 | 415.377 ms | 14.119 s | **33.99x** |

The same campaign completed exact validation on **Orkut: 3,072,441 vertices / 117,185,083 edges / 627,584,181 initial triangles**, establishing 100M-edge-class exact execution. [Methodology and artifacts](docs/ci-scale-evidence.md).

### Same-run published exact reference

VeloGraphX was compared in the same process and on the same hosted runner with the pinned exact `GoldenCounter` reference distributed with public SIGMOD 2021 triangle-counting source code.

| Update batch | VeloGraphX | Published exact reference | Latency ratio |
| --- | ---: | ---: | ---: |
| 1% / 883 edges | 1.066 ms | 43.657 ms | **40.95x lower** |
| 5% / 4,412 edges | 6.782 ms | 47.095 ms | **6.94x lower** |
| 10% / 8,824 edges | 15.495 ms | 53.931 ms | **3.48x lower** |

All **15/15 measured results agreed exactly** across VeloGraphX incremental maintenance, the published exact reference, and VeloGraphX full recomputation. This is specifically a comparison with the pinned exact reference component at revision `1085ba049bb94451661d119284d7cd9b68687a81`, not a claim against the paper's approximate SWTC algorithm. [Full methodology](docs/same-run-published-baseline.md).

### Dynamic storage and long-running consolidation

The current segmented-CSR / packed-delta / row-patch layout was compared with the actual pre-upgrade storage reconstructed from commit `22d05c6b54b9199c852062395b3d6536abca02d9`.

| Current / historical | 10M edges | 100M edges |
| --- | ---: | ---: |
| Loaded RSS | **0.967x** | **0.966x** |
| Mixed update throughput | **1.069x** | **1.628x** |
| Neighbor materialization latency | **0.780x** | **0.783x** |
| Explicit sparse compaction time | **1.657x** | **1.492x** |

The 10M case uses three repetitions; the 100M case is one hosted execution and is treated as scale evidence rather than a low-noise publication result.

Long-running behavior was also measured on checksum-pinned irregular graphs. On directed **web-Google (875,713 vertices / 5,105,039 edges)** after 50 mutation/compaction cycles, owned storage reached **1.332x** the pristine CSR baseline and sampled neighbor latency reached **1.719x**. Validated CSR consolidation reduced storage to **0.751x of the accumulated footprint** and latency to **0.604x of its pre-consolidation value**. All **24 final repetitions** across web-Google and ca-GrQc passed logical-digest, sampled-neighborhood, and edge-count gates.

The steady-state controller was then exercised repeatedly on web-Google and checksum-pinned **com-Orkut (117,185,083 undirected edges / 234,370,166 directed arcs)**. For general graphs the hard owned-storage cap remains **1.25x**. For graphs with at least **100M directed arcs**, a scale-aware helper offers a bounded **1.50x** cap while keeping the latency threshold at 1.25x and retaining persistence/cooldown for latency-driven cutovers.

On the same 60-epoch Orkut runner, the 1.50x policy reduced consolidations from **15 to 6**, total consolidation time from **386.6 s to 156.1 s**, and increased maintenance-amortized throughput from **19.1k to 43.1k ops/s (2.25x)**. The explicit trade-off was a larger owned-storage high-water (**1.529x vs 1.306x**) and about **6.6% higher peak RSS**. Both runs preserved exactly 234,370,166 directed arcs and passed the correctness gates. Compact-source snapshot traversal also avoids per-vertex adjacency-vector materialization during cutover.

This is scale-specific hosted-CI evidence, not a claim that 1.50x is universally optimal. Full canonicalization remains an explicit validated O(E) operation; reducing that structural cost further is still an open systems optimization.

[Storage A/B evidence](docs/storage-ab-evidence.md) · [row-patch accumulation and consolidation](docs/row-patch-accumulation-evidence.md) · [100M+ canonicalization A/B](docs/canonicalization-ab-evidence.md) · [storage architecture](docs/dynamic-storage.md)

### CPU engineering evidence

On a recorded 4-logical-CPU hosted runner, independent-query BFS throughput increased from **5,215.62 queries/s at 1 thread** to **11,105.1 queries/s at 2 threads (2.13x)** and **10,930.3 queries/s at 4 threads**, with identical result digests. The hosted allocation therefore shows useful scaling to two threads and saturation at four.

An adaptive-recomputation ablation places the incremental/full crossover around **20%** on the exercised Facebook workload; at a 50% insertion batch, forcing incremental execution was about **2.5x slower** than full recomputation. This is a decision-boundary experiment, not a claim that the runtime policy always selects the oracle choice.

## What is implemented

| Area | Implementation |
| --- | --- |
| Incremental analytics | BFS / unweighted SSSP, weighted SSSP, exact triangle counting, connected components, k-core, localized PageRank repair |
| Dynamic storage | Segmented CSR, packed sorted deltas, sparse row patches, reverse adjacency, validated CSR consolidation |
| Adaptive execution | Affected-work estimation with incremental-vs-full fallback |
| CPU acceleration | Scalar, AVX2, AVX-512 and ARM NEON intersection paths |
| Multicore / NUMA | Push/pull frontiers, work stealing, topology discovery, affinity and locality-aware policies |
| Compression | Delta, variable-byte, blocked variable-byte and fixed-width adjacency coding |
| Python | pybind11 with NumPy, SciPy CSR and Apache Arrow ingestion |
| Out-of-core infrastructure | Partition files, mmap/fallback reads, bounded cache, async loading and optional Linux `io_uring` prefetch |
| Reproducibility | Dataset checksums, immutable competitor pins, environment capture, correctness gates and retained artifacts |

## Research focus

VeloGraphX is not positioned as novel because it reimplements individual graph algorithms. Its research question is the **architecture-aware integration of incremental maintenance with adaptive recomputation on modern CPUs**: when should a changing graph be repaired locally, and when should the engine switch to full recomputation?

The experimental focus is therefore the incremental-to-full crossover together with storage behavior, reverse dependency traversal, SIMD, multicore scheduling, NUMA placement and compression.

## Correctness and reproducibility

Exact-maintenance experiments are checked against trusted exact results or full recomputation. Iterative algorithms such as PageRank use convergence tolerances and L1/L∞ error against a separately converged full reference.

CI covers Ubuntu and macOS builds, Linux ASan/UBSan, dynamic mutation correctness, forward/reverse storage consistency, optimized-vs-scalar kernels, scheduler/NUMA behavior, compression, native I/O, Python interoperability, dataset checksums and benchmark artifact contracts.

Detailed evidence: [benchmark methodology](docs/benchmark-methodology.md) · [published baseline eligibility](docs/published-baseline-eligibility.md) · [limitations](docs/limitations.md)

## Research boundary

Current results establish reproducible hosted-CI execution, exact large-graph validation, a controlled same-run published exact-reference comparison, 10M/100M storage measurements, real irregular-graph consolidation evidence, repeated steady-state consolidation, and a same-run 100M+-class canonicalization-policy A/B. They do **not** establish universal superiority or full production maturity.

Publication-grade conclusions still require controlled dedicated hardware, repeated 100M+ edge runs across multiple machines/seeds, 8/16/32+ physical-core scaling, genuine multi-socket NUMA experiments, hardware counters, broader irregular graph and update-locality campaigns, structural incremental/segment-level canonicalization, broader same-semantics system comparisons, and independent reproduction.

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

## License

Apache-2.0.
