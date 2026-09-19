<div align="center">

# VeloGraphX

### High-Performance Dynamic Graph Analytics in C++20 + Python

[Docs](https://sauravsingla.github.io/VeloGraphX/) · [Python](python/README.md) · [PyPI](https://pypi.org/project/velographx/) · [Benchmarks](docs/benchmark-methodology.md) · [Paper artifact](PAPER.md) · [Zenodo DOI](https://doi.org/10.5281/zenodo.22842292) · [Workflow catalog](docs/workflow-catalog.md) · [Releases](https://github.com/sauravsingla/VeloGraphX/releases)

[![GitHub Repo stars](https://img.shields.io/github/stars/sauravsingla/VeloGraphX?style=flat&logo=github)](https://github.com/sauravsingla/VeloGraphX/stargazers)
[![PyPI](https://img.shields.io/pypi/v/velographx)](https://pypi.org/project/velographx/)
[![Software release](https://img.shields.io/badge/software-v0.8.2-blue)](https://github.com/sauravsingla/VeloGraphX/releases/tag/v0.8.2)
[![PVLDB submission artifact](https://img.shields.io/badge/PVLDB%20submission%20artifact-v4-purple)](https://github.com/sauravsingla/VeloGraphX/releases/tag/pvldb-2027-submission-v4)
[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

</div>

## About

VeloGraphX is a **high-performance C++20 and Python engine for dynamic graph analytics on large, continuously evolving graphs**. It supports BFS/unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank through dynamic maintenance and full-recomputation paths. Its central systems idea is to keep two semantically equivalent execution choices available for an evolving analytic: **localized maintenance of affected state** and **full recomputation**. A pre-repair policy can choose between them using graph/update structure and prior measured execution cost instead of assuming that either incremental processing or recomputation is always preferable.

The paper-facing claim is deliberately narrow: **the preferred execution strategy changes with graph and update regime, so an evolving-graph engine should expose the repair/recompute crossover as an observable physical-plan choice.** VeloGraphX does not claim universal superiority over other graph systems.

<p align="center">
  <img src="docs/assets/velographx-flow.svg" alt="VeloGraphX dynamic analytics flow" width="90%">
</p>

### Key features

- **Adaptive exact-plan execution for BFS**: choose localized exact repair or exact full recomputation before repair begins; conservative internal fallback remains a separate safety/performance mechanism.
- **Dynamic graph storage**: segmented CSR, packed delta arenas, sparse row patches, forward/reverse adjacency, overlay cancellation, and explicit canonical CSR consolidation.
- **Correctness-first analytics**: exact maintained BFS/unweighted SSSP, connected components, triangle counting, and k-core; weighted SSSP preserves exact distances with conservative recomputation fallback; PageRank uses residual/tolerance validation with conservative fallback rather than a mathematical exactness claim.
- **CPU execution and interoperability**: multicore kernels, compression and partitioning support, graph-access abstractions, a native C++ API, and Python bindings.
- **Reproducible systems evaluation**: checksum-pinned datasets, pinned competitor revisions, explicit timing contracts, exactness gates, retained raw repetitions, machine-readable evidence registries, and documented negative results.

## Publication evidence at a glance

> **Evidence boundary:** GitHub-hosted runs are reproducible hosted evidence. Claims that require stable many-core, NUMA, hardware-counter, NVMe, or machine-specific peak-performance conditions remain outside the headline scope unless separately executed on controlled hardware.

| Evidence | Current audited result |
| --- | --- |
| Primary adaptive BFS selector | **1,610 sequential batch observations** across **9 graph/update regimes** and **45 graph-regime repetitions**; all outputs exact. **3.939% equal-regime mean oracle regret**, **2.309% sample-weighted regret**, **1.739% sample-weighted wrong-arm rate**, and about **0.286 µs** sample-weighted decision cost. The largest `web-Google` regime is retained as a visible tail at **17.477% mean** and **54.424% p95** regret. |
| Dynamic BFS vs NetworKit | `web-Google`: VeloGraphX about **1.38× lower latency**; `ca-GrQc`: NetworKit about **1.35× lower latency**; all **30 paired executions exact**. |
| Dynamic BFS vs RisGraph | In the retained separate `web-Google` campaign, **RisGraph is about 1.90× faster** than VeloGraphX localized repair. This campaign is not combined with the NetworKit campaign into a synthetic ranking. |
| Static BFS / weighted SSSP vs GAP + LAGraph | BFS: VeloGraphX **1.60×–2.04× vs GAP** and **9.4×–11.8× vs LAGraph** in the tested hosted 1–4-thread cases. Weighted SSSP: **GAP wins**; VeloGraphX is **2.6×–3.0× faster than LAGraph** but **7.0×–8.5× slower than GAP**. |
| Exact dynamic triangles vs published exact reference | **15/15 paired comparisons exact**; **40.95× / 6.94× / 3.48× lower median answer-ready latency** than the pinned GoldenCounter exact reference at 1% / 5% / 10% insertion batches on the evaluated workload. |
| 100M+ storage maintenance | On `com-Orkut` (234.4M directed arcs), a bounded 1.50× storage envelope produced **2.25× maintenance-amortized throughput** and **59.6% less consolidation time** than the 1.25× envelope, at about **6.6% higher peak RSS**. |
| Dynamic exactness stress | **2,000,000 updates · 0 BFS mismatches · 0 triangle mismatches** in the retained engineering stress result. |

The authoritative paper-facing mapping from each quantitative statement to its retained run, artifact, checksum, timing contract, and claim boundary is in [PAPER.md](PAPER.md), [paper/results-ledger.md](paper/results-ledger.md), and [benchmarks/paper-evidence.json](benchmarks/paper-evidence.json). Historical development numbers are not substitutes for the current publication-selector result above.

## Getting started

### Python

Install from PyPI:

```bash
python -m pip install velographx
```

Minimal example:

```python
import velographx as vx

g = vx.Graph(4, False)

updates = vx.UpdateBatch()
updates.add(0, 1)
updates.add(1, 2)
g.apply(updates)

bfs = vx.IncrementalBFS(g, 0)
print(bfs.distances)
```

### C++ / build from source

For native C++ development or building VeloGraphX locally:

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Algorithm contracts

| Algorithm | Full / reference | Dynamic / maintained | Contract |
| --- | :---: | :---: | --- |
| BFS / unweighted SSSP | ✓ | ✓ | Exact distances |
| Weighted SSSP | ✓ | ✓ | Exact distances with conservative recomputation fallback |
| Connected components | ✓ | ✓ | Exact maintained connectivity |
| Triangle count | ✓ | ✓ | Exact count |
| k-core | ✓ | ✓ | Exact core-number maintenance |
| PageRank | ✓ | ✓ | Residual/tolerance-validated maintenance with conservative fallback; not presented as mathematically exact |

## Research and benchmarking

VeloGraphX treats benchmark provenance and negative results as part of the system contract. Reviewer-facing references include:

- [Paper artifact guide](PAPER.md)
- [Results ledger](paper/results-ledger.md)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [Hosted native competitor evidence](docs/hosted-native-competitors.md)
- [Published exact triangle baseline](docs/same-run-published-baseline.md)
- [100M+ canonicalization evidence](docs/canonicalization-ab-evidence.md)
- [GraphBolt / DZiG + GAPBS benchmark contract](docs/graphbolt-dzig-gap-benchmark-contract.md)
- [Controlled-hardware execution boundary](docs/controlled-hardware-execution.md)
- [Current limitations](docs/limitations.md)
- [Workflow catalog](docs/workflow-catalog.md)
- [Submission archival status](docs/submission-archive.md)

## Project status and citation

VeloGraphX is an active research and engineering project. APIs may evolve before 1.0; reproducible experiments should pin the exact release tag or commit SHA. `v0.8.2` is the current software release, while `pvldb-2027-submission-v4` is the frozen reviewer/reproducibility snapshot archived at [DOI 10.5281/zenodo.22842292](https://doi.org/10.5281/zenodo.22842292). See [submission archival status](docs/submission-archive.md) for archive and provenance details.

For the software generally:

```bibtex
@software{singla_velographx_2026,
  author  = {Saurav Singla},
  title   = {VeloGraphX},
  year    = {2026},
  url     = {https://github.com/sauravsingla/VeloGraphX},
  license = {Apache-2.0}
}
```

For the exact PVLDB 2027 v4 research artifact:

```bibtex
@software{singla_velographx_pvldb_2027_v4,
  author  = {Saurav Singla},
  title   = {VeloGraphX: Adaptive Exact Analytics for Evolving Graphs},
  year    = {2026},
  version = {pvldb-2027-submission-v4},
  doi     = {10.5281/zenodo.22842292},
  url     = {https://doi.org/10.5281/zenodo.22842292}
}
```

VeloGraphX is licensed under the **Apache License 2.0**. See [LICENSE](LICENSE).
