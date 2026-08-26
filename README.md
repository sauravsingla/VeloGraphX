# VeloGraphX

VeloGraphX is a CPU-native incremental graph analytics engine for large, continuously changing graphs.

The project explores a hybrid CSR + delta storage design, affected-region incremental computation, adaptive CPU graph kernels, multicore execution, NUMA-aware scheduling, and runtime selection between incremental processing and full recomputation.

> Guiding question: when a massive graph changes only slightly, how little work can a modern CPU perform while still producing the correct updated answer?

## Status

VeloGraphX is in early research and implementation. Milestone 0 (research/specification) and Milestone 1 (static graph foundation) are the initial focus. Performance claims are not made until they are backed by reproducible measurements.

## Initial scope

- Modern C++20 core
- CMake build
- CSR graph representation
- Edge-list loading
- BFS
- Connected components
- PageRank
- Triangle counting
- Unit tests and differential tests
- Reproducible benchmark harness
- Research documentation for dynamic storage, incremental execution, SIMD, NUMA, and out-of-core design

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Repository layout

- `include/velographx/` public C++ headers
- `src/` implementation
- `tests/` correctness tests
- `benchmarks/` benchmark code and reports
- `docs/` architecture and methodology
- `docs/research/` research landscape and novelty tracking
- `examples/` usage examples
- `python/` Python bindings (planned)
- `paper/` research-paper working material

## Research discipline

Every proposed optimization should be measured. Every incremental algorithm must be validated against full recomputation. Every claimed novelty should be checked against the literature and public implementations.

## License

Apache-2.0.
