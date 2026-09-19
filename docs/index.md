# VeloGraphX Documentation

VeloGraphX is a C++20 dynamic graph analytics engine with Python bindings. This page is the documentation entry point; the project README remains the concise overview and quick start.

## User guides

- [Architecture](architecture.md)
- [Python bindings](../python/README.md)
- [Benchmark methodology](benchmark-methodology.md)
- [Current limitations](limitations.md)
- [Reproducibility guide](../REPRODUCIBILITY.md)
- [Paper artifact guide](../PAPER.md)
- [Workflow catalog](workflow-catalog.md)
- [Submission archival status](submission-archive.md)
- [Security policy](../SECURITY.md)
- [Contributing](../CONTRIBUTING.md)

## Generated C++ API reference

The public C++ API is generated from `include/velographx/` with Doxygen. From a repository checkout:

```bash
doxygen Doxyfile
```

The generated HTML entry point is:

```text
build/docs/html/index.html
```

The Doxygen configuration includes the public headers and this documentation landing page while excluding implementation-private source files.

## Python API

The Python package exposes the native engine through pybind11. Installation, local builds, interoperability with NumPy/SciPy/Arrow, and supported Python versions are documented in the [Python bindings guide](../python/README.md).

## Research evidence

Publication-facing quantitative claims should be read together with their provenance and limitations. The authoritative claim mapping is maintained in `PAPER.md`, `paper/results-ledger.md`, and `benchmarks/paper-evidence.json`; historical workflow output is not automatically treated as current paper evidence.
