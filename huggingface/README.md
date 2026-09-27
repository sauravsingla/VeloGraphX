---
pretty_name: VeloGraphX Benchmarks
license: apache-2.0
tags:
  - graph-analytics
  - dynamic-graphs
  - graph-processing
  - incremental-computation
  - systems
  - benchmarking
  - reproducibility
  - cpp
  - bfs
  - multicore
  - adaptive-systems
configs:
  - config_name: selector-regimes
    data_files:
      - split: benchmark
        path: data/current-selector-regimes.csv
---

# VeloGraphX Benchmarks

Machine-readable benchmark and reproducibility artifacts for **VeloGraphX**, a C++20 + Python engine for analytics on continuously evolving graphs.

This Hugging Face repository is the benchmark/reproducibility companion to the canonical source repository at [sauravsingla/VeloGraphX](https://github.com/sauravsingla/VeloGraphX). It is published automatically from the GitHub `main` branch using a Hugging Face Trusted Publisher. The bundle is generated only from versioned repository artifacts; it does not invent or synthesize benchmark measurements.

## What VeloGraphX evaluates

VeloGraphX keeps semantically equivalent execution choices available for an evolving analytic: localized maintenance of affected state and full recomputation. For the paper-facing adaptive BFS evaluation, a pre-repair policy chooses between those exact paths using graph/update structure and prior measured execution cost.

The research claim is deliberately scoped: the preferred execution strategy changes with graph and update regime, so repair versus recomputation should be exposed as an observable physical-plan choice. This dataset is not a claim of universal superiority over other graph systems.

## Primary Dataset Viewer table

The Hugging Face Dataset Viewer is configured to load:

`data/current-selector-regimes.csv`

Each row is one evaluated graph/update regime for the current publication selector. Columns include:

- `dataset`
- `batch_size`
- `repetitions`
- `samples`
- `median_batch_us`
- `mean_regret`
- `p95_regret`
- `max_regret`
- `wrong_arm_rate`
- `full_choice_fraction`
- `internal_fallback_fraction`
- `mean_decision_us`

The table contains 9 graph/update regimes spanning `ca-GrQc`, `soc-Epinions1`, and `web-Google`. The associated retained evaluation comprises 1,610 sequential batch observations across 45 graph-regime repetitions, with exact outputs in the retained current-selector evaluation.

## Evidence included

The published bundle contains the evidence needed to trace results back to the source repository:

```text
data/
  current-selector-regimes.csv
  accepted-results.json

artifacts/
  benchmarks/                 benchmark programs, campaign definitions, evidence registries, fixtures
  datasets/                   dataset manifests, provenance plans, benchmark contracts
  paper-data/                 retained paper-facing machine-readable data
  paper/                      results ledger and frozen submission evidence

methodology/
  benchmark-methodology.md
  paper-evidence-index.md
  paper-claims.md
  controlled-hardware-execution.md
  publication-hardware-methodology.md
  limitations.md
  submission-archive.md

reproduction/
  scripts/                    minimal reproduction and dataset-preparation scripts
  tools/                      environment capture, result construction, summaries, validators

_provenance/
  source_manifest.json        source commit plus SHA-256 and size for every exported file

CITATION.cff
LICENSE
PAPER.md
REPRODUCIBILITY.md
```

`artifacts/benchmarks/` contains the versioned benchmark source and machine-readable campaign definitions. It is included because the benchmark code and timing/validation contracts are part of the reproducibility record, not because this Hugging Face repository is intended to replace the software source repository.

## Accepted-results registry

`data/accepted-results.json` is the manuscript-selected results registry. It records provenance and claim boundaries alongside measurements, including retained run IDs, artifact IDs, SHA-256 digests, exactness status, scoped external comparisons, selector results, fallback evidence, ablations, negative results, and limitations.

The registry explicitly distinguishes hosted evidence from universal peak-performance claims and preserves negative results rather than filtering them out.

## Exactness and interpretation

VeloGraphX treats correctness as a first-class benchmark dimension.

For the current retained adaptive BFS selector evidence, compared outputs are exact. The broader project has different algorithm contracts: BFS/unweighted SSSP, weighted SSSP with conservative fallback, connected components, triangle counting, and k-core are maintained under exact-result contracts; PageRank is residual/tolerance validated and is not presented as mathematically exact.

Performance values must be interpreted with their corresponding graph, workload, update regime, timing contract, thread configuration, machine context, and claim boundary. Hosted measurements are scoped hosted evidence. Results requiring stable many-core, NUMA, hardware-counter, NVMe, or other machine-specific conditions are not promoted beyond the controlled-hardware evidence boundary documented in the artifact.

## Visible limitation

The largest retained `web-Google` selector regime is intentionally preserved rather than hidden. Its tail behavior is part of the published evidence. The accepted-results registry also retains held-out and negative-result evidence where the frozen selector does not generalize uniformly.

## Dataset provenance and third-party graphs

This repository does **not** redistribute third-party raw graph datasets merely because VeloGraphX benchmarks refer to them. Dataset manifests retain source/provenance information and benchmark contracts; users must obtain upstream datasets under their original terms where required.

External projects and datasets retain their own licenses. The VeloGraphX software and repository-authored artifact material are distributed under Apache License 2.0 as described in `LICENSE`.

## Reproducing the minimal validation

The canonical GitHub repository contains a short correctness-focused reproduction path:

```bash
git clone https://github.com/sauravsingla/VeloGraphX.git
cd VeloGraphX
sh scripts/reproduce_minimal.sh
```

This builds a focused subset, runs dynamic correctness tests, generates a deterministic evolving graph, executes the adaptive BFS benchmark, records JSON output, and fails if a compared policy disagrees with the exact BFS reference. Timing from that quick path is sanity evidence only, not publication-grade performance evidence.

See `REPRODUCIBILITY.md` and `methodology/benchmark-methodology.md` for the full evidence boundary.

## Loading the primary table

```python
from datasets import load_dataset

regimes = load_dataset(
    "sauravsingla08/velographx-benchmarks",
    "selector-regimes",
)

print(regimes["benchmark"])
```

Or load the CSV directly with pandas:

```python
import pandas as pd

url = "https://huggingface.co/datasets/sauravsingla08/velographx-benchmarks/resolve/main/data/current-selector-regimes.csv"
df = pd.read_csv(url)
print(df)
```

## Frozen research artifact

The frozen reviewer/reproducibility snapshot is:

- **Artifact:** `pvldb-2027-submission-v4`
- **Zenodo DOI:** [10.5281/zenodo.22842292](https://doi.org/10.5281/zenodo.22842292)
- **Software release referenced by the current source repository:** `v0.8.2`

The Hugging Face bundle additionally records the exact Git commit from which each publication was generated in `_provenance/source_manifest.json`.

## Citation

For the frozen research artifact:

```bibtex
@software{singla_velographx_pvldb_2027_v4,
  author  = {Saurav Singla},
  title   = {VeloGraphX: Adaptive Exact Analytics for Evolving Graphs},
  year    = {2026},
  version = {pvldb-2027-submission-v4},
  doi     = {10.5281/zenodo.22842292},
  url     = {https://doi.org/10.5281/zenodo.22842292}
}
```

See `CITATION.cff` for machine-readable citation metadata.

## Canonical locations

- Source code: https://github.com/sauravsingla/VeloGraphX
- Documentation: https://sauravsingla.github.io/VeloGraphX/
- PyPI: https://pypi.org/project/velographx/
- Frozen archive: https://doi.org/10.5281/zenodo.22842292

GitHub remains the authoritative software-development repository. Zenodo remains the frozen archival release. This Hugging Face dataset is the machine-readable benchmark, evidence, and reproducibility layer.
