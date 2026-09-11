# Python bindings

VeloGraphX provides optional pybind11 bindings while keeping the native C++ engine on the performance-critical paths.

## Install from PyPI

Install the published package with:

```bash
python -m pip install velographx
```

Release CI builds and smoke-tests CPython 3.9–3.14 wheels for manylinux x86_64, Windows x64, macOS Intel and macOS Apple Silicon, plus a source distribution.

## Install with pip from a checkout

The repository is packaged with `pyproject.toml` and scikit-build-core, so a local checkout can be built and installed with one command:

```bash
python -m pip install .
```

For an editable developer install:

```bash
python -m pip install -e .
```

The build backend installs pybind11 in an isolated build environment and configures the CMake Python target automatically. A C++20-capable compiler is still required when building from source.

## Manual CMake build

For development or direct CMake use, the previous build path remains supported:

```bash
python -m pip install pybind11
cmake -S . -B build-python -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_TESTS=OFF \
  -DVELOGRAPHX_BUILD_BENCHMARKS=OFF \
  -DVELOGRAPHX_BUILD_PYTHON=ON \
  -Dpybind11_DIR="$(python -m pybind11 --cmakedir)"
cmake --build build-python -j
```

## Interoperability

The bindings include tested interoperability paths for:

- NumPy edge arrays via `from_numpy_edges`;
- SciPy CSR matrices via `from_scipy_csr`;
- Apache Arrow tables via `from_arrow_table`.

CI exercises these adapters alongside dynamic graph operations, incremental BFS, connected components, k-core, PageRank and weighted SSSP bindings.

Ownership, lifetime and dtype behavior are treated as correctness contracts and are validated in CI rather than being described as unverified zero-copy guarantees.

For the main project overview, build instructions and benchmark/evidence boundaries, see the repository [README](../README.md).
