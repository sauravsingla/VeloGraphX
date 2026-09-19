# VeloGraphX — Dynamic Graph Analytics & Incremental Graph Algorithms

VeloGraphX is a **high-performance C++20 and Python engine for dynamic graph analytics and incremental graph algorithms** on large, continuously evolving graphs. It supports exact maintained BFS/unweighted SSSP, connected components, triangle counting and k-core; exact weighted shortest-path distances with conservative recomputation fallback; and PageRank-related maintenance with residual/tolerance validation.

The engine combines mutable graph storage with localized maintenance and full-recomputation paths. Its central systems question is practical: **when should an evolving graph analytic be repaired incrementally, and when should the engine recompute the exact result?**

## Dynamic graph algorithm guides

These focused guides explain the major concepts and APIs while providing stable entry points for developers and researchers searching for dynamic and incremental graph analytics:

- [Dynamic BFS — exact dynamic breadth-first search](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [Incremental BFS — maintain exact BFS after graph updates](https://sauravsingla.github.io/VeloGraphX/incremental_bfs.html)
- [Dynamic graph algorithms in C++20 and Python](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Incremental graph analytics for evolving graphs](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [Dynamic connected components — exact connectivity](https://sauravsingla.github.io/VeloGraphX/dynamic_connected_components.html)
- [Dynamic triangle counting — exact evolving-graph counts](https://sauravsingla.github.io/VeloGraphX/dynamic_triangle_counting.html)
- [C++ graph analytics library with Python bindings](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)

## User guides

- [Architecture](https://github.com/sauravsingla/VeloGraphX/blob/main/docs/architecture.md)
- [Python bindings](https://github.com/sauravsingla/VeloGraphX/blob/main/python/README.md)
- [Benchmark methodology](https://github.com/sauravsingla/VeloGraphX/blob/main/docs/benchmark-methodology.md)
- [Current limitations](https://github.com/sauravsingla/VeloGraphX/blob/main/docs/limitations.md)
- [Reproducibility guide](https://github.com/sauravsingla/VeloGraphX/blob/main/REPRODUCIBILITY.md)
- [Paper artifact guide](https://github.com/sauravsingla/VeloGraphX/blob/main/PAPER.md)
- [Workflow catalog](https://github.com/sauravsingla/VeloGraphX/blob/main/docs/workflow-catalog.md)
- [Submission archival status](https://github.com/sauravsingla/VeloGraphX/blob/main/docs/submission-archive.md)
- [Security policy](https://github.com/sauravsingla/VeloGraphX/blob/main/SECURITY.md)
- [Contributing](https://github.com/sauravsingla/VeloGraphX/blob/main/CONTRIBUTING.md)

## Generated C++ API reference

The public C++ API is generated from `include/velographx/` with Doxygen. From a repository checkout:

```bash
doxygen Doxyfile
python3 tools/build_docs_seo.py build/docs/html
```

The generated HTML entry point is:

```text
build/docs/html/index.html
```

The published site adds page-specific titles and descriptions, canonical URLs, Open Graph and Twitter metadata, a generated `sitemap.xml`, and a `robots.txt` file as part of the GitHub Pages build.

## Python API

The Python package exposes the native engine through pybind11. Installation, local builds, interoperability with NumPy/SciPy/Arrow, and supported Python versions are documented in the [Python bindings guide](https://github.com/sauravsingla/VeloGraphX/blob/main/python/README.md).

Install from PyPI with:

```bash
python -m pip install velographx
```

## Research evidence

Publication-facing quantitative claims should be read together with their provenance and limitations. The authoritative claim mapping is maintained in `PAPER.md`, `paper/results-ledger.md`, and `benchmarks/paper-evidence.json`; historical workflow output is not automatically treated as current paper evidence.
