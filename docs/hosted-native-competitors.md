# Hosted native competitor evidence

This workflow extends the hosted engineering baseline by verifying that pinned native competitors can be built on the same GitHub-hosted Linux runner used for VeloGraphX engineering evidence.

Pinned identities:

- SuiteSparse:GraphBLAS `v10.3.2`
- LAGraph `v1.2.2`
- GAP Benchmark Suite `v1.5`

The workflow captures runner capacity, immutable resolved commit SHAs, and release builds of GraphBLAS, LAGraph, and GAP BFS. It deliberately does **not** convert successful hosted builds into publication-grade timing claims.

The next step after this build-readiness gate is to add normalized BFS runner binaries that satisfy the existing `tools/native_competitors/*_wrapper.py` contract, then execute same-runner correctness-checked timing through `tools/competitor_benchmark.py`.

Hosted runner results remain `research_claim: false` and `publication_grade: false`. Dedicated hardware is still required for Issue #10 publication-grade scaling, NUMA, counter, and native competitor claims.
