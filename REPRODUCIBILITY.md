# Reproducibility Guide

This guide provides a short path for validating the central VeloGraphX research artifact without requiring the full publication-scale benchmark campaign.

## Minimal validation

### Requirements

- CMake 3.20 or newer
- a C++20-capable compiler
- Python 3
- a Unix-like shell

Run:

```bash
sh scripts/reproduce_minimal.sh
```

By default the script uses:

- build directory: `build-paper-minimal/`
- output directory: `artifacts/paper-minimal/`

Both locations can be overridden:

```bash
VELOGRAPHX_BUILD_DIR=/tmp/vx-build \
VELOGRAPHX_ARTIFACT_DIR=/tmp/vx-artifacts \
sh scripts/reproduce_minimal.sh
```

## What the minimal script validates

The script intentionally focuses on correctness and the adaptive-execution contract rather than headline performance.

It performs the following steps:

1. configures a Release CMake build with tests and benchmarks enabled;
2. builds a focused set of incremental/dynamic correctness tests plus the adaptive-policy benchmark;
3. runs tests covering incremental execution, deletion repair, randomized dynamic updates, and execution planning;
4. generates a deterministic synthetic directed graph with a stable initial connected structure and a later update stream;
5. runs `velographx_adaptive_policy_bfs` over that evolving graph;
6. records the benchmark's JSON output; and
7. fails if the benchmark reports that any compared policy disagrees with the exact BFS reference.

The generated JSON contains results for:

- `always_incremental`
- `always_full`
- `simple_threshold`
- `adaptive`
- a per-batch measured oracle

Timing from this quick script is **sanity evidence only**. It is not intended to support publication-grade performance comparisons because the machine is not controlled by the artifact.

## Outputs

After a successful run, inspect:

```text
artifacts/paper-minimal/adaptive_policy.json
artifacts/paper-minimal/synthetic-evolving-graph.txt
```

The JSON should contain:

```json
"all_policies_exact": true
```

The script also prints a compact summary of each policy's total and mean batch time so a reviewer can verify that the execution paths ran successfully.

## Full experimental record

The minimal path above is deliberately small. The broader experimental contracts and evidence are documented separately:

- [Benchmark methodology](docs/benchmark-methodology.md)
- [Paper claim map](docs/paper-claims.md)
- [Ablation study](docs/ablation-study.md)
- [Hosted native competitor evidence](docs/hosted-native-competitors.md)
- [External dynamic baselines](docs/external-dynamic-baselines.md)
- [Canonical publication campaign](docs/canonical-publication-campaign.md)
- [Controlled-hardware execution](docs/controlled-hardware-execution.md)
- [Current limitations](docs/limitations.md)

## Reproducibility principles

For performance results intended for a paper, VeloGraphX distinguishes between:

- correctness validation,
- hosted engineering measurements,
- same-machine external comparisons, and
- dedicated controlled-hardware publication evidence.

A result should be promoted to a publication claim only when its dataset provenance, software revisions, timing boundary, exactness checks, repetition policy, machine details, and retained artifacts satisfy the corresponding benchmark contract.
