# Current limitations

VeloGraphX implements a broad dynamic-graph systems stack, but the **paper-facing evidence is deliberately narrower than the implementation**. This document is synchronized with the current manuscript and separates what is demonstrated from what remains open.

## Selector generalization

The main paper studies repair-versus-recompute selection most deeply for exact dynamic BFS.

The historical three-graph program contains 1,610 exact adaptive batch observations and low average oracle regret, but it is not a fresh out-of-sample generalization result. The frozen no-retuning holdout makes the limitation explicit:

- unseen `Amazon0312`: 1.52% equal-regime mean regret;
- timestamp-ordered `CollegeMsg`: 34.52% equal-regime mean regret, with a much larger worst-regime tail.

The architecture therefore generalizes more strongly than the current hand-designed selector. VeloGraphX does **not** claim a universal near-oracle policy.

## Production fallback frequency

The production-style replay uses the normal 0.35 affected-region fallback and is exact across 93 aligned observations.

All six observed fallback-only fallbacks occur in the declared 220K destructive-cascade stress case. The 72 retained observations from `ca-GrQc`, `soc-Epinions1`, and `web-Google` trigger no 0.35 internal fallback.

The experiment demonstrates that a pre-repair choice can avoid already-started repair work when fallback would occur; it does **not** estimate the natural frequency of such fallbacks in real workloads.

## Incremental algorithm scope

Localized or maintained paths exist for BFS / unweighted SSSP, weighted SSSP, triangle counting, connected components, k-core, and PageRank-related workflows.

The strongest mathematical exactness argument and adaptive-plan evaluation in the paper are for BFS. Weighted SSSP may conservatively recompute after destructive weighted changes. PageRank is residual/tolerance validated with conservative fallback and is **not** presented as mathematically exact.

A selector policy that works for BFS is not assumed to transfer unchanged to every analytic.

## External-system comparisons

External comparisons are intentionally scoped to compatible timing and correctness contracts.

- The accepted NetworKit campaign uses NetworKit 11.2.1, one thread, two graph families, three fixed roots per dataset, and five paired repetitions per root. VeloGraphX wins the retained `web-Google` workload while NetworKit wins `ca-GrQc`.
- The retained RisGraph result comes from a separate hosted campaign and is not merged with NetworKit into an absolute three-system ranking.
- The matched GraphBolt experiment uses a pinned artifact runtime and the same retained mutation stream. Its GraphBolt/VeloGraphX answer-ready ratio is 14.219x at 0.1%, 2.245x at 1%, and 0.886x at 5% operation fraction, so the winner reverses at the largest tested fraction.
- GAP/LAGraph results are hosted 1--4-thread context. VeloGraphX wins the evaluated BFS cases, while GAP wins weighted SSSP.

These results support workload-specific crossover claims, not universal system rankings.

## Hardware scope

The engine contains multicore scheduling, topology discovery, affinity/NUMA machinery, SIMD kernels, compression, partition files, asynchronous loading, and optional Linux `io_uring` prefetch.

The current paper does **not** claim:

- stable 8/16/32+ core scaling;
- controlled multi-socket NUMA superiority;
- hardware-counter advantages;
- research-scale NVMe/out-of-core superiority;
- universal SIMD or codec thresholds; or
- machine-independent peak throughput.

Those claims require dedicated controlled hardware and same-machine competitor runs. Hosted evidence is sufficient for the paper's scoped exactness, crossover, ablation, and paired relative-comparison claims because the manuscript states that boundary explicitly.

## Storage evidence

The large storage result is a measured time/memory trade-off on `com-Orkut`, not a universal canonicalization policy. A wider 1.50x storage envelope reduced consolidation frequency and time and improved maintenance-amortized throughput at higher peak memory, but other graphs and hardware may prefer different bounds.

## Temporal semantics

The held-out `CollegeMsg` campaign preserves observed interaction arrival order. Repeated interactions are idempotent under VeloGraphX's simple-graph semantics, and sliding-window removals are induced expiries rather than observed deletion events.

It is therefore a genuine temporal-order stress test of the selector, but not a model of every temporal-graph semantics.

## Python/API maturity

Python bindings support NumPy, SciPy CSR, and Apache Arrow ingestion when optional dependencies are available. The package is still pre-1.0 and the public API is not yet a frozen long-term compatibility surface.

## Reproducibility and archival status

The repository records accepted evidence through run IDs, artifact IDs, digests, timing contracts, exactness gates, and machine-readable ledgers.

The submission-freeze workflow now also copies the selected core raw evidence bundles out of GitHub Actions retention into the immutable paper release, verifies their recorded SHA-256 digests, anonymously re-downloads the public source archive, and performs a clean-room minimal reproduction.

The external archival dependency is now satisfied for `pvldb-2027-submission-v4`: the submission artifact is published on Zenodo at [DOI 10.5281/zenodo.22842292](https://doi.org/10.5281/zenodo.22842292), and `CITATION.cff` records that identifier. This DOI identifies the archived software/reproducibility artifact; it is not a venue-publication DOI and does not imply PVLDB acceptance.

## Workflow history

The repository intentionally retains historical research workflows because their names and run IDs are part of the provenance record. They are not all current merge gates or recommended entry points.

See [`workflow-catalog.md`](workflow-catalog.md) for the supported core workflows, current paper-evidence workflows, and historical/development workflow families.
