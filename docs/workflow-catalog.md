# Workflow catalog

VeloGraphX intentionally keeps historical research campaigns in the repository because workflow names, run IDs, artifact IDs, and hashes are part of the experimental provenance. This file separates the **supported current entry points** from **historical/development campaigns** so reviewers and contributors do not need to infer status from the 70 workflow files under `.github/workflows/`.

## Core supported workflows

These workflows define normal repository health, packaging, and submission gates.

| Workflow | Purpose |
| --- | --- |
| `ci.yml` | Linux/macOS build and test, sanitizer coverage, dataset/tooling checks, Python interoperability |
| `security.yml` | secret-pattern scan, SPDX SBOM, CodeQL |
| `tsan.yml` | thread-sanitizer coverage for concurrency-sensitive code |
| `python-package.yml` | Python wheel/package validation |
| `publish-pypi.yml` | software-package publication |
| `benchmark-preflight.yml` | benchmark contract/preflight checks |
| `publication-artifact.yml` | paper evidence and artifact-contract validation |
| `public-dataset-plan.yml` | public-dataset plan contract |
| `owens-benchmark-contract.yml` | benchmark-methodology contract |
| `pvldb-paper.yml` | official PVLDB manuscript build and page gate |
| `submission-freeze-release.yml` | immutable paper freeze, retained-evidence archive, clean-room verification, optional Zenodo publication |

A pull request is considered repository-ready only when the applicable core checks are green. The submission freeze is a separate push-to-`main` operation because it creates an immutable versioned tag and release.

## Current paper-evidence reproduction workflows

These are the reviewer-facing workflows for the results used by the current manuscript. They are reproducibility entry points, not a request to rerun every historical campaign.

| Workflow | Current role |
| --- | --- |
| `publication-selector-cross-dataset.yml` | primary 1,610-observation selector campaign |
| `publication-production-fallback.yml` | production 0.35 fallback / double-work campaign |
| `publication-selector-heldout.yml` | frozen Amazon0312 + timestamp-ordered CollegeMsg holdout |
| `publication-selector-feature-ablation.yml` | clean one-factor selector ablation |
| `publication-graphbolt-real-comparison.yml` | matched GraphBolt dynamic-BFS comparison |
| `publication-selector-tail-validation.yml` | focused selector-tail regression support |
| `external-networkit-native-multidataset.yml` | accepted paired NetworKit dynamic-BFS evidence |
| `external-risgraph-baseline.yml` / `risgraph-baseline.yml` | retained RisGraph evidence and reproduction path |
| `same-run-published-baseline.yml` | published exact triangle-reference comparison |
| `storage-ab-evidence.yml` / `canonicalization-ab.yml` | large-graph storage/canonicalization evidence |

The authoritative mapping from claims to retained runs and artifact hashes is **not this catalog**. Use `PAPER.md`, `paper/results-ledger.md`, `paper/data/accepted-results.json`, `paper/submission-closure-evidence.json`, and `benchmarks/paper-evidence.json`.

## Historical and development workflows

The remaining workflows are intentionally retained because they document selector development, earlier calibration/holdout rounds, benchmark experiments, or capability probes. Typical historical families include:

- `adaptive-policy-*`, `adaptive-selector-*`, `selector-v2-*`, and `bfs-selector-v3-*`;
- older hosted/capacity/crossover probes such as `hosted-*`, `current-capacity-validation.yml`, and `multi-dataset-crossover.yml`;
- implementation-specific evidence such as `row-patch-accumulation.yml`, `steady-state-storage.yml`, compression/CPU scaling, and external-container experiments;
- one-off reviewer-closure and issue-specific campaigns.

They remain under `.github/workflows/` rather than being moved into an archive directory because moving them would break stable workflow URLs and make historical run provenance harder to follow. Most are manual or narrowly triggered and are **not required merge gates**.

Historical numbers are never promoted merely because the workflow still exists. A result becomes paper evidence only after exactness/provenance audit and registration in the paper evidence ledger.

## Which workflow should I run?

For ordinary development, run the normal CI and security checks through a pull request. For the paper, start from the specific claim in `paper/results-ledger.md` and use the workflow named there. Do not rerun all 70 workflows as a generic validation step.

For a new submission snapshot, update `paper/submission-freeze.json` to a **new immutable tag** and merge only after the core PR checks pass. Never move an existing submission tag.
