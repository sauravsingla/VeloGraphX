# VeloGraphX Paper Artifact Guide

## Working research framing

**VeloGraphX: Adaptive Exact Analytics for Evolving Graphs**

VeloGraphX studies a systems question that arises in continuously changing graphs: **when should an exact analytical result be maintained by localized incremental repair, and when is full recomputation the better execution strategy?**

The repository implements both choices behind a common dynamic graph substrate and exposes an adaptive policy that can select between them while preserving the exactness contract of the underlying algorithm.

This file is a reviewer-facing map of the research artifact. It is not a substitute for a submitted manuscript and it deliberately separates implemented capability from the evidence scope of each quantitative claim.

## Core contributions represented by the artifact

### C1. Mutable graph substrate for evolving analytics

VeloGraphX provides segmented CSR storage with mutable delta state, sparse row patches, forward/reverse adjacency, and explicit canonical consolidation. The design supports repeated graph updates without rebuilding a canonical CSR representation after every batch.

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

The adaptive BFS benchmark evaluates four policies over the same evolving workload:

1. always incremental,
2. always full recomputation,
3. a simple threshold policy, and
4. the VeloGraphX adaptive policy.

It also emits a per-batch oracle derived from the measured policies and verifies every policy against a fresh exact BFS reference.

Primary references:

- [`benchmarks/adaptive_policy_bfs.cpp`](benchmarks/adaptive_policy_bfs.cpp)
- [Ablation study](docs/ablation-study.md)
- [Benchmark methodology](docs/benchmark-methodology.md)

### C4. Reproducible evidence with explicit claim boundaries

The project treats dataset identity, timing boundaries, competitor revisions, exactness, retained artifacts, and negative results as part of the experimental contract. Hosted same-run evidence can support **scoped relative manuscript claims** when systems or policies share the documented runner/timing envelope and correctness contract. It is not used as proof of universal peak performance, many-core scaling, or hardware-specific superiority.

Primary references:

- [Paper evidence index](docs/paper-evidence-index.md)
- [Machine-readable paper evidence registry](benchmarks/paper-evidence.json)
- [Benchmark methodology](docs/benchmark-methodology.md)
- [Current limitations](docs/limitations.md)
- [Hosted native competitor evidence](docs/hosted-native-competitors.md)

## Minimal reviewer validation

On a Unix-like environment with CMake, a C++20 compiler, and Python 3:

```bash
sh scripts/reproduce_minimal.sh
```

The script builds a focused set of correctness tests, creates a deterministic synthetic evolving graph, executes the adaptive-policy benchmark, checks that all compared policies remain exact, and writes machine-readable output under `artifacts/paper-minimal/`.

See [REPRODUCIBILITY.md](REPRODUCIBILITY.md) for the exact contract and the larger evidence map.

## Manuscript evidence map

The authoritative manuscript-facing result registry is:

- [docs/paper-evidence-index.md](docs/paper-evidence-index.md)
- [`benchmarks/paper-evidence.json`](benchmarks/paper-evidence.json)

These files record the accepted hosted campaigns, GitHub Actions run IDs, artifact IDs/hashes, repetition/exactness contracts, safe claims, and explicit overclaim boundaries. They also identify results that remain pending audit and therefore must not be promoted into headline manuscript numbers.

## Paper claim map

A compact claim-to-implementation-to-evidence matrix is maintained in:

- [docs/paper-claims.md](docs/paper-claims.md)

The purpose of that table is to make it easy to distinguish:

- what is implemented,
- what has accepted hosted same-run or paired evidence,
- what has external baseline evidence,
- what remains pending artifact audit, and
- which hardware-specific claims would require a different experimental environment.

## Evidence boundary

The repository should not be read as claiming that incremental execution always wins, that VeloGraphX is universally faster than competing systems, or that hosted CI establishes universal multicore/NUMA performance. The retained benchmark record includes regimes and workloads where recomputation or an external system wins.

For this paper, dedicated hardware is **optional rather than blocking** unless the manuscript chooses to make many-core, multi-socket NUMA, hardware-counter, NVMe, or microarchitecture-specific peak-performance claims. The core paper can instead rely on exactness, repair/recompute crossover, oracle-relative policy quality, same-run relative comparisons, reproducibility, and scoped 1–4-thread hosted evidence.

The intended research claim is narrower: **the preferred exact execution strategy changes with workload and graph regime, so a dynamic analytics engine should expose and adapt to that crossover rather than hard-code one execution mode.**
