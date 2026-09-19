# VeloGraphX Documentation

VeloGraphX is a C++20 dynamic graph analytics engine with Python bindings. This page is the documentation entry point; the project README remains the concise overview and quick start.

The generated API documentation is published through the repository's GitHub Pages workflow.

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
```

The generated HTML entry point is:

```text
build/docs/html/index.html
```

The Doxygen configuration includes the public headers and this documentation landing page while excluding implementation-private source files.

## Python API

The Python package exposes the native engine through pybind11. Installation, local builds, interoperability with NumPy/SciPy/Arrow, and supported Python versions are documented in the [Python bindings guide](https://github.com/sauravsingla/VeloGraphX/blob/main/python/README.md).

## Research evidence

Publication-facing quantitative claims should be read together with their provenance and limitations. The authoritative claim mapping is maintained in `PAPER.md`, `paper/results-ledger.md`, and `benchmarks/paper-evidence.json`; historical workflow output is not automatically treated as current paper evidence.
