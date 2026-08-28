# Multi-dataset incremental crossover campaign

This campaign extends the hosted-CI incremental triangle-maintenance evidence beyond `ca-GrQc` to structurally different public graphs. It is **engineering evidence only** and remains explicitly `research_claim=false`; it is not a publication-grade performance claim.

## Dataset families

The campaign uses three additional public graph structures:

| Dataset | Family / role | Source identity |
| --- | --- | --- |
| `ca-HepTh` | larger collaboration graph | immutable Git revision + verified Git blob |
| `facebook-combined` | denser social graph | immutable Git revision + verified Git blob |
| `p2p-Gnutella08` | peer-to-peer network graph | immutable Git revision + verified Git blob |

Canonical dataset provenance is recorded in `datasets/multi-dataset-crossover.json`. The benchmark downloads only immutable revision URLs, verifies the expected Git blob identity before use, and records a SHA-256 digest of the acquired bytes in the result artifact.

## Methodology

Each source edge list is deterministically normalized to a simple undirected graph because the current incremental triangle benchmark evaluates undirected triangle maintenance. Self-loops are removed, reverse/duplicate edges are deduplicated, and vertex IDs are remapped contiguously.

For each normalized graph, `velographx_public_update_fraction_benchmark` runs five independent repetitions at each configured update fraction:

`1e-6, 1e-5, 1e-4, 1e-3, 0.01, 0.05, 0.10, 0.20, 0.50, 0.75, 1.00, 1.50, 2.00`

Each repetition starts from the same base graph and generates a deterministic batch of previously absent edges. The campaign records both the requested number of updates and the number actually changed. The run fails if those values differ.

For every measurement, the incremental triangle result is compared exactly with a full recomputation. A single disagreement fails the benchmark and therefore fails CI.

## Reported statistics

`tools/summarize_update_crossover.py` emits a machine-readable JSON summary containing, for each update fraction:

- number of samples;
- requested and actual changed-edge counts;
- median and p95 incremental time;
- median and p95 full-recompute time;
- median and p95 speedup;
- correctness rate.

The summary also records graph vertex/base-edge counts and the first tested fraction whose median speedup is less than or equal to `1.0`. If none is observed, the result explicitly states that no crossover was reached within the configured sweep.

## CI and provenance

`.github/workflows/multi-dataset-crossover.yml` captures the GitHub-hosted runner environment, validates benchmark preflight metadata, runs the normal project test suite before measurement, validates all measurement contracts, builds a provenance-rich result bundle, and uploads the complete per-dataset artifact directory.

Hosted-runner timings can be useful for correctness, crossover-shape exploration, regression detection and campaign plumbing. They must not be interpreted as evidence of superiority over other graph engines or as dedicated-hardware scalability results.

A publication-grade follow-up should rerun the same artifact contract on controlled hardware with pinned compiler/runtime settings, dedicated CPU placement, larger public graphs, native competitors and enough repetitions for stable uncertainty estimates.
