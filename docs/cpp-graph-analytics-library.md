\page cpp_graph_analytics_library C++ Graph Analytics Library with Python Bindings

VeloGraphX is a C++20 graph analytics library for dynamic and evolving graphs, with Python bindings for users who want native graph-processing performance from Python workflows.

The library combines mutable graph storage, maintained graph algorithms, multicore execution support, reproducible benchmarking, and explicit algorithm-specific correctness contracts.

## C++20 graph analytics engine

The native implementation is written in modern C++20. Public headers cover graph storage, incremental analytics, runtime scheduling, partitioning, compression-related support, and other engine components.

Maintained analytics include exact BFS/unweighted SSSP, exact connected components, exact triangle counting, exact k-core maintenance, exact weighted shortest-path distances with conservative recomputation fallback, and PageRank-related maintenance with residual/tolerance validation.

## Dynamic and incremental graph algorithms

VeloGraphX is designed for graphs that change over time. Its dynamic storage layer allows repeated update batches without requiring a complete canonical CSR rebuild after every change.

For BFS, the research system keeps localized exact repair and exact full recomputation available as competing execution plans. A pre-repair selector can choose between them based on graph/update structure and observed execution cost.

This makes VeloGraphX useful for developers studying incremental graph algorithms as well as for systems researchers evaluating when maintenance should replace or complement recomputation.

## Python bindings

The Python package exposes the native engine through pybind11. Install it from PyPI with:

```bash
python -m pip install velographx
```

Python users can construct graphs, apply update batches, run maintained analytics, and interoperate with common scientific Python data structures while the core graph processing remains native.

## Research and engineering focus

VeloGraphX is both a software library and a reproducible graph-systems research artifact. The repository contains tests, sanitizers, benchmark harnesses, pinned external-comparison contracts, raw retained evidence, machine-readable result registries, and a Zenodo-archived submission artifact.

The project deliberately avoids claiming universal superiority. Its documentation retains workloads where another system, full recomputation, or the oracle plan performs better.

## Getting started

For C++ development, clone the repository and build with CMake:

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For Python, install the `velographx` package and follow the examples in the repository's Python guide.

## Related VeloGraphX documentation

- [Dynamic graph algorithms](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Incremental graph analytics](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [Dynamic BFS](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [Incremental BFS](https://sauravsingla.github.io/VeloGraphX/incremental_bfs.html)
- [Dynamic connected components](https://sauravsingla.github.io/VeloGraphX/dynamic_connected_components.html)
- [Dynamic triangle counting](https://sauravsingla.github.io/VeloGraphX/dynamic_triangle_counting.html)
