# Python bindings

VeloGraphX provides optional pybind11 bindings while keeping the native C++ engine on the performance-critical paths.

## Build

Install pybind11, then configure with the Python bindings enabled:

```bash
cmake -S . -B build-python -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_TESTS=OFF \
  -DVELOGRAPHX_BUILD_BENCHMARKS=OFF \
  -DVELOGRAPHX_BUILD_PYTHON=ON \
  -Dpybind11_DIR="$(python -m pybind11 --cmakedir)"
cmake --build build-python -j
```

The resulting `velographx` extension is built from the same C++ engine used by the native API.

## Interoperability

The bindings include tested interoperability paths for:

- NumPy edge arrays via `from_numpy_edges`;
- SciPy CSR matrices via `from_scipy_csr`;
- Apache Arrow tables via `from_arrow_table`.

CI exercises these adapters alongside dynamic graph operations, incremental BFS, connected components, k-core, PageRank and weighted SSSP bindings.

Ownership, lifetime and dtype behavior are treated as correctness contracts and are validated in CI rather than being described as unverified zero-copy guarantees.

For the main project overview, build instructions and benchmark/evidence boundaries, see the repository [README](../README.md).
