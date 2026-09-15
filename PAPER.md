# VeloGraphX Paper Artifact Guide

## Working research framing

**VeloGraphX: Adaptive Exact Analytics for Evolving Graphs**

VeloGraphX studies a systems question that arises in continuously changing graphs: **when should an exact analytical result be maintained by localized incremental repair, and when is full recomputation the better execution strategy?**

The repository implements both choices behind a common dynamic graph substrate and exposes a pre-repair policy that can select between them while preserving the exactness contract of the underlying algorithm.

This file is a reviewer-facing map of the research artifact. It deliberately separates implemented capability from the evidence scope of each quantitative claim.

## Manuscript workspace

The current paper materials are maintained under [`paper/`](paper/):

- [Working manuscript](paper/manuscript.md)
- [Figure/table and claim ledger](paper/results-ledger.md)
- [Machine-readable selected results](paper/data/accepted-results.json)
- [Current-selector per-regime CSV](paper/data/current-selector-regimes.csv)
- [Related-work and novelty boundary](paper/related-work-notes.md)
- [Core bibliography](paper/references.bib)

## Core contributions represented by the artifact

### C1. Mutable graph substrate for evolving analytics

VeloGraphX provides segmented CSR storage with mutable delta state, sparse row patches, forward/reverse adjacency, and explicit canonical consolidation. The design supports repeated graph updates without rebuilding canonical CSR after every batch.

Primary references:

- [Architecture](docs/architecture.md)
- [Dynamic storage design](docs/dynamic-storage.md)
- [Canonicalization evidence](docs/canonicalization-ab-evidence.md)

### C2. Exact maintained analytics

The codebase contains maintained or dynamic execution paths for BFS / unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank-related workflows. Localized execution can fall back to full recomputation when repair is unsafe or no longer economical.

Primary references:

- [`include/velographx/incremental/`](include/velographx/incremental/)
- [Architecture](docs/architecture.md)
- [Current limitations](docs/limitations.md)

### C3. Adaptive repair-versus-recompute execution

The publication harness evaluates the same exact evolving-BFS result under:

1. always incremental,
2. always full recomputation,
3. a simple update-density threshold,
4. a history-cost baseline, and
5. the current `publication-preflight-v1` policy.

It also emits a per-batch oracle defined from the exact incremental/full alternatives, verifies every policy against fresh BFS outside timing, and records selector decision cost, wrong-arm choices, and internal fallbacks.

Primary references:

- [`benchmarks/publication_policy_bfs.cpp`](benchmarks/publication_policy_bfs.cpp)
- [Cross-dataset publication selector workflow](.github/workflows/publication-selector-cross-dataset.yml)
- [Results ledger](paper/results-ledger.md)
- [Benchmark methodology](docs/benchmark-methodology.md)

### C4. Reproducible evidence with explicit claim boundaries

The project treats dataset identity, timing boundaries, competitor revisions, exactness, retained artifacts, and negative results as part of the experimental contract. Hosted same-run evidence can support **scoped relative manuscript claims** when systems or policies share the documented runner/timing envelope and correctness contract. It is not used as proof of universal peak performance, many-core scaling, or hardware-specific superiority.

Primary references:

- [Paper evidence index](docs/paper-evidence-index.md)
- [Machine-readable paper evidence registry](benchmarks/paper-evidence.json)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [Current limitations](docs/limitations.md)

## Current publication-selector evidence

The current selector now has two audited retained artifacts.

The primary cross-dataset run is GitHub Actions run `34929398888`, artifact `10381490811`, SHA-256 `a78a421663b08228a7bd26596f9ad0498e2ad4a0c1470c79471dcaef97885a7b`. It covers `ca-GrQc`, `soc-Epinions1`, and `web-Google`, three regimes per graph, five repetitions per regime, one thread, and 1,610 adaptive batch samples. All outputs are exact. The current policy records 3.939% mean oracle regret across regimes, 2.309% sample-weighted regret, 1.739% sample-weighted wrong-arm rate, zero internal fallbacks, and about 0.286 µs sample-weighted selector decision cost.

The tail is intentionally retained: the largest evaluated `web-Google` regime records 17.477% mean regret and 54.424% p95 regret. The manuscript must not replace this current result with stronger historical development numbers.

A focused `web-Google` regression run (`34928935983`, artifact `10381480310`, SHA-256 `800d53a327900b4a579bb761d07adf86e3622b4e46a1a8ddea0fb25603bf3691`) independently validates exactness and the removal of redundant one-sided-full behavior.

## Minimal reviewer validation

On a Unix-like environment with CMake, a C++20 compiler, and Python 3:

```bash
sh scripts/reproduce_minimal.sh
```

The script builds a focused set of correctness tests, creates a deterministic synthetic evolving graph, executes the adaptive-policy benchmark, checks that all compared policies remain exact, and writes machine-readable output under `artifacts/paper-minimal/`.

For the larger current-policy matrix, run the manual workflow [`.github/workflows/publication-selector-cross-dataset.yml`](.github/workflows/publication-selector-cross-dataset.yml).

See [REPRODUCIBILITY.md](REPRODUCIBILITY.md) for the larger evidence map.

## Manuscript evidence map

The authoritative manuscript-facing registries are:

- [docs/paper-evidence-index.md](docs/paper-evidence-index.md)
- [`benchmarks/paper-evidence.json`](benchmarks/paper-evidence.json)
- [`paper/results-ledger.md`](paper/results-ledger.md)

These files record accepted campaigns, run IDs, artifact IDs/hashes, repetition/exactness contracts, safe claims, negative results, tail limitations, and explicit overclaim boundaries.

## Novelty boundary

VeloGraphX does **not** claim invention of dual-path incremental/static graph execution, history-based cost selection, dependency-driven affected-region processing, sparsity-aware incremental processing, or low-latency exact evolving-graph analytics. Prior work covering those ideas is summarized in [`paper/related-work-notes.md`](paper/related-work-notes.md).

The paper's defensible contribution is narrower: exact repair and full recomputation are exposed as competing physical execution strategies over one mutable substrate, and a pre-repair policy uses structural state plus observed cost to avoid both unnecessary global work and repair-discovery work that would subsequently fall back to full execution.

## Evidence boundary

The repository should not be read as claiming that incremental execution always wins, that VeloGraphX is universally faster than competing systems, or that hosted CI establishes universal multicore/NUMA performance. The retained benchmark record includes regimes and workloads where recomputation or an external system wins, and the current selector retains a visible tail-regret regime.

For this paper, dedicated hardware is **optional rather than blocking** unless the manuscript chooses to make many-core, multi-socket NUMA, hardware-counter, NVMe, or microarchitecture-specific peak-performance claims. The core paper can rely on exactness, repair/recompute crossover, current-selector oracle-relative quality, same-run relative comparisons, reproducibility, and scoped hosted evidence.

The intended research claim is: **the preferred exact execution strategy changes with workload and graph regime, so a dynamic analytics engine should expose and adapt to that crossover rather than hard-code one execution mode.**
