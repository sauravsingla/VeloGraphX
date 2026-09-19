# Changelog

## Unreleased

## 0.8.2 - 2026-09-09

- Added production-ready Python packaging with `scikit-build-core` and `pybind11` for the existing C++20 engine.
- Published `velographx` to PyPI with validated wheels for CPython 3.9–3.14 across manylinux x86_64, Windows x64, macOS Intel, and macOS Apple Silicon, plus a source distribution.
- Added installed-wheel and source-distribution smoke validation, including dynamic BFS correctness and package/version checks.
- Added secure PyPI publication through GitHub Actions OIDC Trusted Publishing and the protected `pypi` environment instead of a long-lived API token.
- Expanded the Python-accessible surface for graph updates, incremental BFS, connected components, triangle counting, k-core, PageRank, and weighted SSSP while preserving the existing native C++ architecture.

## 0.8.1 - 2026-09-03

- Improved CI and release-workflow reliability following the v0.8.0 release.
- Prevented release-tag pushes from unintentionally triggering Adaptive BFS research and performance-validation workflows.
- Restricted applicable Adaptive BFS validation workflows to their intended branch/path triggers and moved the legacy selector calibration workflow to manual execution.
- Preserved existing benchmark acceptance thresholds, correctness checks, and frozen research workflows.
- Kept the v0.8.0 core dynamic-graph functionality and public APIs unchanged; this release was workflow/CI maintenance rather than an algorithmic change.

## 0.8.0 - 2026-09-03

- Replaced the dynamic graph's `vector<vector<VertexId>>` compact base with fixed-size segmented CSR storage and contiguous zero-copy rows.
- Replaced per-vertex `unordered_set` insertion/deletion overlays with sorted slices in shared packed delta arenas, including overlay cancellation and fragmentation repacking.
- Added explicit reverse adjacency with matching reverse deltas and `in_neighbors()` / `compact_in_neighbors()` APIs.
- Updated localized PageRank repair to traverse actual predecessors instead of scanning every graph vertex for each active destination.
- Upgraded full PageRank to tolerance-based convergence with explicit dangling-mass redistribution and L1/L-infinity iteration residuals.
- Added quantitative localized-vs-full PageRank validation with L1/L-infinity rank-vector error, reference convergence metadata, and an explicit validation-only fallback mode.
- Added conservative full recomputation when an update changes dangling status or materially changes the rank of a dangling vertex, because dangling mass is a graph-wide PageRank dependency.
- Exposed PageRank residuals, convergence state, validation metrics, and validated-update mode through the Python bindings.
- Added dynamic-storage introspection for compact edge count, live delta count, approximate owned storage, and storage-layout documentation.
- Expanded dynamic graph tests for directed reverse traversal, packed overlays, compaction equivalence, overlay cancellation, and cross-segment vertex growth.
- Expanded PageRank tests for reverse-indexed localized repair, converged full-reference comparison, L1/L-infinity error contracts, dangling-node semantics, and validation fallback.

## 0.7.0 - 2026-08-28

- Added reproducible hosted-CI benchmark evidence with explicit non-publication claim gates.
- Added multi-dataset incremental crossover campaigns with immutable dataset provenance, exact correctness checks, repeated statistics, and machine-readable artifacts.
- Added publication-grade controlled-hardware campaign contracts covering thread scaling, genuine NUMA placement, hardware counters, ablations, competitor pinning, and readiness validation.
- Added public-dataset verification and pinned benchmark metadata, including web-Google planning and smaller public graph campaigns.
- Added configurable update-fraction sweeps for incremental-vs-recompute crossover analysis.
- Expanded CI validation for benchmark contracts, publication readiness, public datasets, and hosted engineering evidence.
- Aligned README and limitations documentation with currently implemented and validated capabilities.
- Preserved a strict distinction between hosted-CI engineering evidence and future controlled-hardware publication claims.

## 0.3.0 - 2026-08-26

- Added versioned CSR+delta dynamic graph storage with batch updates and compaction.
- Added incremental triangle counting, insertion-optimized components, BFS, unweighted SSSP, PageRank repair and k-core baseline.
- Added adaptive intersection, CPU feature detection, sparse/dense frontier and push/pull policies.
- Added observable incremental-vs-recompute execution planning.
- Added portable thread pool, aligned allocation, compression baseline, memory-budget and NUMA abstractions.
- Added binary graph I/O and optional pybind11 bindings.
- Added dynamic/kernel tests, benchmark harnesses, research notes, ablation plan and paper outline.

## 0.1.0

- Initial research specification and static C++ graph foundation.
