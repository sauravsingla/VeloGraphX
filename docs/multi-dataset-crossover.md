# Multi-dataset incremental crossover campaign

This campaign extends VeloGraphX incremental triangle-maintenance evidence across three structurally different public graphs. It is **GitHub-hosted engineering evidence only** and remains explicitly `research_claim=false`; it is not a publication-grade performance claim.

## Validated public-dataset snapshot

Workflow run `33247205294` on commit `f7e8068807c930723d41b6929239b2e33baf5d9a` completed successfully for all three datasets. Each dataset passed immutable acquisition, deterministic normalization, the project Release build and test suite, benchmark preflight, 13 update fractions × 5 repetitions, exact incremental-vs-full triangle-count correctness for every measurement, provenance-rich result-bundle validation, and artifact upload.

| Dataset | Family | Normalized graph | 1% updates | 5% updates | 10% updates |
| --- | --- | ---: | ---: | ---: | ---: |
| `facebook-combined` | social | 4,039 vertices / 88,234 edges | **84.77x** | **17.90x** | **8.90x** |
| `ca-HepTh` | collaboration | 9,877 vertices / 25,973 edges | **59.05x** | **12.20x** | **6.59x** |
| `p2p-Gnutella08` | peer-to-peer network | 6,301 vertices / 20,777 edges | **55.23x** | **11.53x** | **6.27x** |

Values are median `full_recompute_time / incremental_time` over five repetitions at each fraction. Correctness rate was **100% for every sample** in all three complete sweeps.

No median crossover was observed within the configured sweep through a changed-edge batch equal to 200% of the original base-edge count. That observation is useful for engineering/hypothesis development, but it must not be generalized beyond these datasets, update-generation rules, implementation state, and GitHub-hosted runners.

## Dataset families and provenance

| Dataset | Family / role | Source identity |
| --- | --- | --- |
| `ca-HepTh` | larger collaboration graph | immutable Git revision + verified Git blob |
| `facebook-combined` | denser social graph | immutable Git revision + verified Git blob |
| `p2p-Gnutella08` | peer-to-peer network graph | immutable Git revision + verified Git blob |

Canonical provenance is recorded in `datasets/multi-dataset-crossover.json`. The workflow downloads immutable revision URLs, verifies the expected Git blob identity before use, and records a SHA-256 digest of the acquired bytes in each result artifact.

## Methodology

Each source edge list is deterministically normalized to a simple undirected graph because the current incremental triangle benchmark evaluates undirected triangle maintenance. Self-loops are removed, reverse/duplicate edges are deduplicated, and vertex IDs are remapped contiguously.

For each normalized graph, `velographx_public_update_fraction_benchmark` runs five repetitions at each configured update fraction:

`1e-6, 1e-5, 1e-4, 1e-3, 0.01, 0.05, 0.10, 0.20, 0.50, 0.75, 1.00, 1.50, 2.00`

Each repetition starts from the same base graph and generates a deterministic batch of previously absent edges. The campaign records requested and actually changed edges and fails if those values differ.

For every measurement, the incremental triangle result is compared exactly with a full recomputation. A single disagreement fails the benchmark and CI.

The README intentionally reports only the 1%, 5%, and 10% points. Tiny fractions are excluded from headline reporting because nanosecond/microsecond-scale incremental timings can produce visually extreme ratios and are less robust as summary evidence.

## Reported statistics

`tools/summarize_update_crossover.py` emits a machine-readable JSON summary containing, for every update fraction:

- number of samples;
- requested and actual changed-edge counts;
- median and p95 incremental time;
- median and p95 full-recompute time;
- median and p95 speedup;
- correctness rate;
- crossover status.

## CI and provenance

`.github/workflows/multi-dataset-crossover.yml` captures the hosted runner environment, validates benchmark preflight metadata, runs the normal project test suite before measurement, validates the complete measurement contract, builds a provenance-rich result bundle, and uploads the complete per-dataset artifact directory.

The workflow now retriggers when core `src/**`, `include/**`, benchmark, relevant dataset/tooling, CMake, or workflow files change so the evidence cannot silently remain stale after benchmark-affecting changes.

Hosted-runner timings are useful for correctness, crossover-shape exploration, regression detection, and reproducible engineering evidence. They must not be interpreted as evidence of universal superiority over other graph engines or as dedicated-hardware scalability results.

A publication-grade follow-up should rerun the same artifact contract on controlled hardware with pinned compiler/runtime settings, dedicated CPU placement, larger public graphs, native competitors, and enough repetitions for stable uncertainty estimates.
