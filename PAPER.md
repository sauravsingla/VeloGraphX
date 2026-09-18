# VeloGraphX Paper Artifact Guide

## Working research framing

**VeloGraphX: Adaptive Exact Analytics for Evolving Graphs**

VeloGraphX studies a systems question that arises in continuously changing graphs: **when should an exact analytical result be maintained by localized incremental repair, and when is full recomputation the better execution strategy?**

The repository implements both choices behind a common dynamic graph substrate and exposes a pre-repair policy that can select between them while preserving the exactness contract of the underlying algorithm.

This file is a reviewer-facing map of the research artifact. It deliberately separates implemented capability from the evidence scope of each quantitative claim and retains negative generalization results rather than tuning them away.

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

### C2. Maintained analytics with explicit semantics

The codebase contains maintained or dynamic execution paths for BFS / unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank-related workflows. BFS/unweighted SSSP, connected components, triangles and k-core use exact-result contracts; weighted SSSP retains exact distances through conservative recomputation fallback; PageRank is described separately as residual/tolerance-validated maintenance with conservative fallback rather than as mathematically exact.

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

It emits a per-batch oracle defined from the exact incremental/full alternatives, verifies every policy against fresh BFS outside timing, and records selector decision cost, wrong-arm choices, and internal fallbacks.

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

The primary cross-dataset run is GitHub Actions run `34929398888`, artifact `10381490811`, SHA-256 `a78a421663b08228a7bd26596f9ad0498e2ad4a0c1470c79471dcaef97885a7b`. It covers `ca-GrQc`, `soc-Epinions1`, and `web-Google`, three regimes per graph, five repetitions per regime, one thread, and 1,610 adaptive batch samples. All outputs are exact. The current policy records 3.939% mean oracle regret across regimes, 2.309% sample-weighted regret, 1.739% sample-weighted wrong-arm rate, and about 0.286 µs sample-weighted selector decision cost.

The tail is intentionally retained: the largest evaluated `web-Google` regime records 17.477% mean regret and 54.424% p95 regret. The manuscript must not replace this current result with stronger historical development numbers.

A focused `web-Google` regression run (`34928935983`, artifact `10381480310`, SHA-256 `800d53a327900b4a579bb761d07adf86e3622b4e46a1a8ddea0fb25603bf3691`) independently validates exactness and the removal of redundant one-sided-full behavior.

## Submission-closure evidence added on 2026-09-17

These campaigns were executed after the selector source was frozen. They are retained because they directly answer the principal pre-submission questions; none is used to retune the selector after seeing the result.

### Production 0.35 fallback / double-work replay

- **Run:** `35237513376`
- **Artifact:** `10503926632`
- **Artifact SHA-256:** `26c37720fef6524ee68e816a095f7dd591c5cfd93c2f112a8de7ebd8874a621a`
- **Exactness:** all 93 aligned observations exact
- **Fallback-only internal fallbacks:** 6
- **Fallback opportunities avoided by the frozen pre-repair selector:** 6
- **Remaining selector-path internal fallbacks:** 0
- **False-full choices:** 33
- **Conservatively observed repair-discovery-then-full work:** 17.323 ms
- **Conservatively observed double work avoided by pre-repair selection:** 17.323 ms

The 220K destructive-cascade case is a mechanism stress case and must not be presented as a natural workload. Real-graph rows remain separately available in the retained artifact. This result directly demonstrates the mechanism the primary oracle-arm harness intentionally could not measure: pre-repair selection can avoid work that fallback-only execution pays before recomputing.

### Frozen-selector held-out evaluation

- **Run:** `35237513595`
- **Artifact:** `10504131673`
- **Artifact SHA-256:** `3cc4394693818450523fc56a9bb4368f7e26365d19077f2835a915a2bc312452`
- **Post-result retuning:** none
- **Exactness:** all retained outputs exact

Across 15 held-out regimes, equal-regime mean regret is **21.32%**, worst-regime mean regret **185.83%**, and worst-regime p95 regret **311.88%**. The result is heterogeneous rather than uniformly strong: unseen `Amazon0312` is low-regret (six regimes, 1.52% equal-regime mean; 2.29% worst-regime mean; 9.44% worst-regime p95), while the genuine timestamp-ordered `CollegeMsg` stream is difficult (nine regimes, 34.52% equal-regime mean; 185.83% worst-regime mean; 311.88% worst-regime p95).

This negative result is now part of the paper boundary. The manuscript must **not** claim that `publication-preflight-v1` generalizes uniformly to unseen temporal workloads. The stronger defensible systems result is architectural: both exact physical plans remain available and observable, while the current hand-designed selector itself is not a solved universal policy.

`CollegeMsg` preserves timestamp arrival order. Repeated interactions are idempotent under simple-graph semantics, and sliding-window removals are induced expiries rather than observed deletion events; that distinction must remain explicit.

### Clean frozen feature ablation

- **Run:** `35237513377`
- **Artifact:** `10504591544`
- **Artifact SHA-256:** `a0aa985cd3f64d2e340938ba0111ec017076e24412665213bd4a9f4d5bf3601d`
- **Exactness:** all variants exact

| Policy | Equal-regime mean regret | Worst-regime mean | Worst-regime p95 |
| --- | ---: | ---: | ---: |
| `adaptive` | 4.82% | 18.76% | 58.39% |
| `adaptive_no_structural` | 8.16% | 42.16% | 131.88% |
| `adaptive_no_affected` | 4.20% | 17.53% | 56.07% |
| `adaptive_no_uncertainty` | 142.97% | 1160.11% | 3558.37% |
| `simple_threshold` | 17.24% | 59.65% | 141.10% |
| `always_incremental` | 11.45% | 59.03% | 131.93% |
| `always_full` | 632.00% | 1756.90% | 4451.34% |

The ablation supports two concrete statements under this frozen harness: structural preflight materially reduces regret/tails, and the uncertainty guard is essential to prevent catastrophic overconfident full choices. Removing the previous-affected-work factor does **not** improve the causal case for that feature in this campaign; its slightly lower aggregate regret must be retained as a negative/neutral result rather than hidden.

### Matched real-dataset GraphBolt comparison

- **Run:** `35237513587`
- **Artifact:** `10503851688`
- **Artifact SHA-256:** `6d3a756040276aa4c49f4820bf31d3d0b60025affcc024cd9bb04cb2eddc9b59`
- **Exactness:** VeloGraphX exact; GraphBolt final BFS independently verified on every retained run
- **Paired repetitions:** five per operation fraction

| Operation fraction | GraphBolt / VeloGraphX answer-ready latency ratio |
| --- | ---: |
| 0.01% | 14.219× |
| 0.1% | 2.245× |
| 0.5% | 0.886× |

A ratio above 1 means lower VeloGraphX latency; below 1 means lower GraphBolt latency. The winner reversal at the largest evaluated fraction is retained. Both systems consume the same checksum-retained mutation stream on the same hosted allocation. Because GraphBolt uses its pinned legacy artifact runtime, this is a scoped same-run result rather than a universal system ranking.

## Minimal reviewer validation

On a Unix-like environment with CMake, a C++20 compiler, and Python 3:

```bash
sh scripts/reproduce_minimal.sh
```

The script builds a focused set of correctness tests, creates a deterministic synthetic evolving graph, executes the adaptive-policy benchmark, checks that all compared policies remain exact, and writes machine-readable output under `artifacts/paper-minimal/`.

For the primary policy matrix use [`.github/workflows/publication-selector-cross-dataset.yml`](.github/workflows/publication-selector-cross-dataset.yml). Submission-closure workflows are also retained under `.github/workflows/` for production fallback replay, held-out evaluation, clean feature ablation, and the matched GraphBolt campaign.

## Manuscript evidence map

The authoritative manuscript-facing registries are:

- [docs/paper-evidence-index.md](docs/paper-evidence-index.md)
- [`benchmarks/paper-evidence.json`](benchmarks/paper-evidence.json)
- [`paper/results-ledger.md`](paper/results-ledger.md)

These files record accepted campaigns, run IDs, artifact IDs/hashes, repetition/exactness contracts, safe claims, negative results, tail limitations, and explicit overclaim boundaries.

## Novelty boundary

VeloGraphX does **not** claim invention of dual-path incremental/static graph execution, history-based cost selection, dependency-driven affected-region processing, sparsity-aware incremental processing, or low-latency exact evolving-graph analytics. Prior work covering those ideas is summarized in [`paper/related-work-notes.md`](paper/related-work-notes.md).

The paper's defensible contribution is narrower: exact repair and full recomputation are exposed as competing physical execution strategies over one mutable substrate, and a pre-repair policy uses structural state plus observed cost to avoid both unnecessary global work and repair-discovery work that would subsequently fall back to full execution. The new held-out result further narrows the policy claim: the architecture is reusable, but the current selector should not be represented as universally generalizing.

## Submission freeze and DOI boundary

The intended immutable submission tag is `pvldb-2027-submission-v3`, defined in [`paper/submission-freeze.json`](paper/submission-freeze.json). Zenodo metadata is maintained in [`.zenodo.json`](.zenodo.json), and the repository contains a submission-artifact release workflow.

A DOI must **not** be invented or inserted before an external DOI-capable archive actually mints it. The repository-side freeze path is complete; DOI publication additionally requires the configured Zenodo token or GitHub–Zenodo release integration documented by the workflow.

## Evidence boundary

The repository should not be read as claiming that incremental execution always wins, that VeloGraphX is universally faster than competing systems, that the current selector is uniformly near-oracle on unseen workloads, or that hosted CI establishes universal multicore/NUMA performance. The retained benchmark record includes regimes and workloads where recomputation, an external system, or the oracle wins, including the difficult held-out `CollegeMsg` temporal workload.

For this paper, dedicated hardware is optional rather than blocking unless the manuscript chooses to make many-core, multi-socket NUMA, hardware-counter, NVMe, or microarchitecture-specific peak-performance claims. The core paper can rely on exactness, the repair/recompute crossover, direct production-fallback evidence, scoped selector quality, transparent generalization limits, same-run external comparisons, and reproducibility.

The intended research claim is: **the preferred exact execution strategy changes with workload and graph regime, so a dynamic analytics engine should expose repair and recomputation as observable physical plans rather than hard-code one execution mode; the selector that chooses among them must itself be evaluated as a workload-dependent systems component.**
