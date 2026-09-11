<div align="center">

# VeloGraphX

### Exact Dynamic Graph Analytics for Evolving Graphs

<hr width="78%">

<h3 align="center">
  <a href="docs/architecture.md">Architecture</a> |
  <a href="python/README.md">Python</a> |
  <a href="docs/benchmark-methodology.md">Benchmarks</a> |
  <a href="https://github.com/sauravsingla/VeloGraphX/releases">Releases</a> |
  <a href="https://github.com/sauravsingla/VeloGraphX/discussions">Discussions</a>
</h3>

[![GitHub Repo stars](https://img.shields.io/github/stars/sauravsingla/VeloGraphX?style=flat&logo=github)](https://github.com/sauravsingla/VeloGraphX/stargazers)
[![PyPI](https://img.shields.io/pypi/v/velographx)](https://pypi.org/project/velographx/)
[![Release](https://img.shields.io/github/v/release/sauravsingla/VeloGraphX)](https://github.com/sauravsingla/VeloGraphX/releases/latest)
[![CI](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml/badge.svg)](https://github.com/sauravsingla/VeloGraphX/actions/workflows/ci.yml)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/w/sauravsingla/VeloGraphX)](https://github.com/sauravsingla/VeloGraphX/graphs/commit-activity)
[![GitHub contributors](https://img.shields.io/github/contributors/sauravsingla/VeloGraphX)](https://github.com/sauravsingla/VeloGraphX/graphs/contributors)
[![Good first issues](https://img.shields.io/github/issues/sauravsingla/VeloGraphX/good%20first%20issue?label=good%20first%20issues&color=7057ff)](https://github.com/sauravsingla/VeloGraphX/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

⭐ **If VeloGraphX helps you analyze evolving graphs faster, [give us a star](https://github.com/sauravsingla/VeloGraphX) — it helps more developers and researchers discover the project.**

</div>

## About

VeloGraphX is a **high-performance C++20 engine for graph analytics on large, continuously evolving graphs**.

Instead of assuming incremental computation is always faster, VeloGraphX explicitly decides whether to **repair only the affected state** or **recompute the result from scratch**, based on update locality, affected work, graph scale, and observed execution cost.

This makes VeloGraphX useful for workloads where graph structure changes continuously and analytical results need to stay current — including relationship networks, fraud and transaction graphs, knowledge graphs, infrastructure graphs, dependency networks, and graph-systems research.

<p align="center">
  <img src="docs/assets/velographx-flow.svg" alt="VeloGraphX dynamic analytics flow" width="90%">
</p>

### Key features

- **Adaptive incremental execution**: Automatically choose between localized repair and full recomputation instead of assuming one strategy always wins.
- **Dynamic graph storage**: Segmented CSR, packed delta arenas, sparse row patches, forward/reverse adjacency, overlay cancellation, and explicit canonical CSR consolidation.
- **Correctness-first analytics**: Dynamic and maintained execution paths for BFS / unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and validated PageRank maintenance.
- **High-performance CPU execution**: Multicore scheduling, SIMD-oriented intersections, NUMA-aware policies, compression, partition caching, and asynchronous partition loading.
- **Storage-independent algorithms**: Graph-access abstractions decouple analytics from the native storage layer and enable execution over CSR and foreign graph representations.
- **C++ engine with Python API**: Performance-critical execution stays in native C++20 while Python bindings provide a convenient interface for applications and experimentation.
- **Reproducible systems evaluation**: Pinned datasets and competitors, correctness gates, machine-readable artifacts, explicit timing contracts, and documented negative results.

## Results at a Glance

> **Benchmark note:** GitHub-hosted measurements are reproducible engineering evidence, not publication-grade controlled-hardware claims. VeloGraphX intentionally reports both wins and losses.

| Evidence | Verified result |
| --- | --- |
| Dynamic exactness stress | **2,000,000 updates · 0 BFS mismatches · 0 triangle mismatches** |
| Adaptive BFS selector | **108/108 exact · 1.66% mean overhead from regime-best** |
| GraphBolt / DZiG comparison | **15.35× / 4.28× / 2.33× faster** on tested tiny / medium / large hosted update regimes |
| Three-system dynamic BFS campaign | **91/91 exact** evaluated VeloGraphX results |
| Dynamic BFS vs NetworKit | `web-Google`: **~1.38× faster VeloGraphX** · `ca-GrQc`: **~1.35× faster NetworKit** |
| Multicore scaling at 4 threads | BFS **2.74×** · CC **2.50×** · triangles **2.24×** |
| Compression | **3.25×–3.78× smaller**, with a documented traversal-performance trade-off |
| Epinions multi-root BFS | **~1.74× faster** than NetworKit by aggregate mean batch latency in the hosted 1T campaign |

The benchmark record deliberately includes cases where competitors win. GAP is substantially faster on the tested static SSSP workload, NetworKit wins some dynamic regimes, CSR remains preferable for some full-recomputation paths, and compression currently exchanges traversal speed for lower memory use.

See the [benchmark methodology](docs/benchmark-methodology.md), [competitor benchmarking](docs/competitor-benchmarking.md), and [limitations](docs/limitations.md) for claim boundaries and reproduction details.

## Getting Started

Install VeloGraphX from PyPI:

```bash
python -m pip install velographx
```

Then create and update a graph:

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

The Python package uses the same native **C++20 engine** underneath.

Release CI builds and smoke-tests CPython **3.9–3.14** wheels across Linux, Windows, macOS Intel, and Apple Silicon, together with a validated source distribution.

For more setup options and examples, see:

- [Python API and installation](python/README.md)
- [C++ examples](examples/)
- [Architecture](docs/architecture.md)
- [Dynamic storage design](docs/dynamic-storage.md)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [Competitor benchmarking](docs/competitor-benchmarking.md)
- [Security](SECURITY.md)
- [Release notes](CHANGELOG.md)

<details>
<summary><b>Build the C++ engine from source</b></summary>

Source builds require **CMake ≥ 3.20** and a **C++20 compiler**.

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run the examples:

```bash
./build/velographx_example
./build/velographx_dynamic_example
```

Minimal C++ API:

```cpp
#include "velographx/algorithms.hpp"

velographx::CsrGraph graph(
    {{0, 1}, {1, 2}, {2, 3}},
    false
);

auto distance = velographx::bfs_distances(graph, 0);
auto triangles = velographx::triangle_count(graph);
```

</details>

## Algorithms

| Algorithm | Full / Reference | Dynamic / Incremental | Contract |
| --- | :---: | :---: | --- |
| BFS / unweighted SSSP | ✓ | ✓ | Exact distances |
| Weighted SSSP | ✓ | ✓ | Exact distances with conservative recomputation fallback |
| Connected components | ✓ | ✓ | Exact maintained connectivity |
| Triangle count | ✓ | ✓ | Exact count |
| k-core | ✓ | ✓ | Exact core-number maintenance |
| PageRank | ✓ | ✓ | Localized maintenance with residual/tolerance validation and conservative fallback |

## Research and Benchmarking

VeloGraphX treats benchmarking as part of the system design rather than only as a headline performance exercise.

The repository includes deterministic update streams and correctness validation, pinned public datasets and competitor revisions, repeated measurements and machine-readable benchmark artifacts, incremental-vs-recompute crossover experiments, storage and algorithm ablation studies, controlled-hardware experiment specifications, thread-scaling and NUMA experiment plans, and explicit publication and evidence boundaries.

Useful references:

- [Benchmark methodology](docs/benchmark-methodology.md)
- [Ablation study](docs/ablation-study.md)
- [GraphBolt / DZiG + GAPBS benchmark contract](docs/graphbolt-dzig-gap-benchmark-contract.md)
- [Three-system dynamic BFS campaign](docs/three-system-dynamic-bfs-campaign.md)
- [Canonical publication campaign](docs/canonical-publication-campaign.md)
- [Controlled-hardware execution](docs/controlled-hardware-execution.md)
- [Limitations](docs/limitations.md)

## Contributing

We welcome contributions and collaborations around **dynamic graph algorithms, CPU optimization, mutable graph storage, benchmark reproducibility, interoperability, Python APIs, and documentation**.

See the [Contributing Guide](CONTRIBUTING.md) to get started.

New to VeloGraphX? Browse the [good first issues](https://github.com/sauravsingla/VeloGraphX/issues?q=is%3Aissue%20is%3Aopen%20label%3A%22good%20first%20issue%22) or join [GitHub Discussions](https://github.com/sauravsingla/VeloGraphX/discussions) to share a workload, idea, benchmark, or feature proposal.

## Technical Feedback

VeloGraphX's evaluation methodology has benefited from technical and benchmarking feedback from [John D. Owens](https://github.com/jowens), [Scott Beamer](https://github.com/sbeamer), [Timothy A. Davis](https://github.com/DrTimothyAldenDavis), [Brian Wheatman](https://github.com/wheatman), and Mikhail Kirilin.

Their feedback is acknowledged as technical input and **does not imply endorsement of VeloGraphX or its benchmark results**.

## Project Status

VeloGraphX is an **active research and engineering project**.

The current package release is **v0.8.2**. APIs may continue to evolve before 1.0, so users running reproducible experiments should pin a release or commit.

The project currently provides a C++20 engine, a standard pip-installable Python package, cross-platform release wheels, reproducible benchmark infrastructure, and correctness-focused dynamic graph analytics.

## Citation

If you use VeloGraphX in research, please cite the repository and the specific release used in your experiments.

Citation metadata is maintained in [`CITATION.cff`](CITATION.cff).

```bibtex
@software{singla_velographx_2026,
  author  = {Saurav Singla},
  title   = {VeloGraphX},
  year    = {2026},
  url     = {https://github.com/sauravsingla/VeloGraphX},
  license = {Apache-2.0}
}
```

For reproducibility, include the VeloGraphX release tag or commit SHA used for your experiments.

## License

VeloGraphX is licensed under the **Apache License 2.0**.

See the [LICENSE](LICENSE) file for details.
