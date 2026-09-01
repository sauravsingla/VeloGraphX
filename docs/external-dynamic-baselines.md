# External dynamic baselines

VeloGraphX admits an external system to the main dynamic-BFS comparison only when the system can be pinned immutably, receives the same graph/update semantics, restores an exact BFS answer after every batch, and passes correctness checks before timing is accepted. The goal is to avoid broad claims from mismatched benchmark contracts.

The machine-readable eligibility record is [`benchmarks/external-baseline-manifest.json`](../benchmarks/external-baseline-manifest.json).

## Comparison classes

1. **Same-semantics dynamic-BFS baselines** receive the same ordered graph stream, root, initial window, batch boundaries, insertions/deletions, and answer-ready timing contract. They may appear in the dynamic-BFS latency evidence.
2. **Secondary dynamic-graph systems** support structural updates and graph analytics but do not expose the same exact maintained-BFS contract. They are kept separate from the latency table.

The accepted same-semantics systems are **RisGraph** and **NetworKit DynBFS**.

## RisGraph

The native RisGraph comparison pins commit `4e77f774d4aa7cd0bf3011e713496573b70c91ab` and uses checksum-pinned directed `web-Google` with the source-order sliding-window workload: 99% initial import, 4,096-edge batches, deterministic root `481807`, and 13 update batches. The final exact BFS reaches `588,118` vertices.

Evidence run: GitHub Actions `33286241439`, VeloGraphX head `e3f6c21133a43a2c6826709c336e56b846c98252`, artifact `9724535579`.

| System / policy | Mean answer-ready batch time |
| --- | ---: |
| VeloGraphX deletion-aware repair | **59.658 ms** |
| VeloGraphX legacy full recomputation | **118.039 ms** |
| RisGraph | **31.333 ms** |

VeloGraphX deletion-aware repair is **1.98x faster than its former full-recompute deletion policy**, with 0/13 safety fallbacks, but **RisGraph remains about 1.90x faster than VeloGraphX** on this run. Exact final BFS layer histograms agree across the systems and independent full-BFS checks.

This remains a separate hosted-runner campaign and therefore must not be numerically merged with the NetworKit campaign into a three-system league table.

## Native C++ NetworKit repeated campaign

NetworKit is pinned to **11.2.1**, Git commit `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`. The workflow builds the C++ core from source and runs VeloGraphX and `NetworKit::DynBFS` sequentially on the **same GitHub-hosted runner**.

The timed envelope includes graph mutation plus restoration of the dynamic BFS answer. Fresh full-BFS verification is outside the timed region. `OMP_NUM_THREADS=1` and `OMP_PROC_BIND=true` are applied. Each dataset uses the same three frozen, reachability-screened roots for both systems and runs **five paired repetitions per root**. Every repetition must match exact full BFS and pass a nontrivial final-reachability gate.

Accepted evidence run: **GitHub Actions `33542995289`**, VeloGraphX head `d042a993896e0b21be7dd6b9717895a0f4213430`, artifact **`9814639042`**, artifact SHA-256 `acf743bcac2542660fa050d70105e5e1e5f79d9e22ef1ec7324cf1e147ae5f12`.

### web-Google

Source SHA-256: `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1`. Normalized stream SHA-256: `f451add10bfea6cdae5b7030410e0e93acbf5c4fc5f0821738b9863f0e9c6496`.

- 875,713 vertices / 5,105,039 directed edges
- 99% initial import = 5,053,988 edges
- batch size 4,096 / 13 batches
- fixed roots 481807, 771121 and 391806
- final reachable vertices 588,118 / 588,118 / 588,283; acceptance floor 100,000

| Root | VeloGraphX mean | NetworKit mean | Mean paired VX / NK |
| ---: | ---: | ---: | ---: |
| 481807 | **29.560 ms** | 40.412 ms | **0.732×** |
| 771121 | **30.064 ms** | 43.185 ms | **0.697×** |
| 391806 | **21.923 ms** | 28.776 ms | **0.762×** |
| Mean of root means | **27.182 ms** | 37.458 ms | **0.730×** |

VeloGraphX is faster at every tested root. The mean of the three per-root paired ratios is **0.7301×**, corresponding to about **27.0% less answer-ready batch time** on this hosted runner and frozen workload.

### ca-GrQc

The checksum-pinned source has 5,242 vertices and 28,980 rows. The provenance gate verifies 12 self-loops plus 14,484 unique non-loop undirected relationships represented reciprocally. Self-loops are removed and each verified relationship is emitted once in each direction.

Source SHA-256: `513efa8bb5c6d3d739797ca028d4a26a7df6bc20adcf3e722e18d1bcdb0e62d5`. Derived symmetric-stream SHA-256: `b6e16b41991049365670ac6407055034d2e29e21c8b000b9ecb0ff0ac8964192`.

- 5,242 vertices / 28,968 derived directed edges
- 75% initial import = 21,726 edges
- batch size 256 / 29 batches
- fixed roots 4282, 2465 and 1974
- final reachable vertices 3,119 at every root; acceptance floor 1,000

| Root | VeloGraphX mean | NetworKit mean | Mean paired VX / NK |
| ---: | ---: | ---: | ---: |
| 4282 | 110.680 µs | **82.732 µs** | 1.340× |
| 2465 | 111.434 µs | **79.109 µs** | 1.409× |
| 1974 | 112.378 µs | **86.391 µs** | 1.301× |
| Mean of root means | 111.497 µs | **82.744 µs** | 1.350× |

NetworKit is faster at every tested root. The mean of the three per-root paired ratios is **1.3497×**. The result preserves the earlier conclusion on this small graph while showing it is not an artifact of one selected root.

The focused ca-GrQc optimization campaign preserved BFS semantics while attacking fixed overhead. The effective-neighbor traversal is allocation-free and exactly merges compact CSR, row patches and live deltas; BFS workspaces reuse repair heap storage and flat generation-stamped update-key tables; packed-delta mutation avoids duplicate forward lookup; and percentage-triggered automatic global delta maintenance now requires at least **16,384 live delta entries**. A controlled same-run 4,096-vs-16,384 maintenance-floor A/B measured ca-GrQc at **272.045 µs vs 115.771 µs** (candidate/base `0.4256`) while web-Google slightly improved from **35.542 ms to 35.303 ms**. All A/B executions were exact and all 27 candidate tests passed. The accepted maintenance change is commit `7a4083656cc2dfe67903efe7bdc7d822a337bd3e`; A/B artifact `9728969220`, SHA-256 `ee50b010ef5c52e9402465968499d22d7bdaa77d6da3f6268ab928f7f2795b68`.

Relative to the earlier clean ca-GrQc VeloGraphX mean of **0.3884 ms**, the final canonical mean of **0.1110 ms** is about **71% lower**. Relative to the immediately preceding `0.3134 ms` campaign, it is about **65% lower**. The frozen workload, roots, batch sizes, thread settings, competitor revision and exactness gates were unchanged.

### Superseded VeloGraphX-only multi-root evidence

The earlier workflow selected the same roots deterministically using graph support and final-graph reachability only; benchmark timing was not used for root selection. It ran VeloGraphX alone and established the frozen root set used by the canonical same-machine comparison above.

Evidence run: GitHub Actions `33301366020`, artifact `9729058306`, artifact SHA-256 `cab9eec6489c8b9c65d57b2b781a5241a1954a5d5e99e94577c9ce0c737cccfa`.

| Dataset / root | Final reachable | Mean batch |
| --- | ---: | ---: |
| `ca-GrQc` / 4282 | 3,119 | **103.546 µs** |
| `ca-GrQc` / 2465 | 3,119 | **105.039 µs** |
| `ca-GrQc` / 1974 | 3,119 | **108.345 µs** |
| `web-Google` / 481807 | 588,118 | **30.699 ms** |
| `web-Google` / 771121 | 588,118 | **30.986 ms** |
| `web-Google` / 391806 | 588,283 | **22.691 ms** |

All six roots passed exact incremental-vs-full validation and the dataset-specific nontrivial reachability gate. This artifact remains useful root-selection provenance, but the new canonical campaign supersedes it for VeloGraphX/NetworKit comparison claims.

## Interpretation

The multi-root campaign supersedes the previous single-root native NetworKit campaign as the primary NetworKit evidence. It preserves the same conclusion at all three roots of each dataset: VeloGraphX wins on web-Google and NetworKit wins on ca-GrQc. All 30 paired executions are exact.

These are **hosted-CI engineering measurements**, not publication-grade universal conclusions. Pairwise same-run ratios are more informative than absolute comparisons across different hosted runners. Publication-level conclusions still require dedicated hardware, additional graph families/update regimes, multicore scaling, hardware counters, and independent reproduction.

## Serious systems screened but excluded from the same-semantics table

- **Teseo / GFE Driver** — strong structural dynamic-graph evaluation, but its public contract separates update experiments from Graphalytics BFS rather than maintaining exact BFS after every identical batch.
- **Aspen** — supports streaming updates and BFS over acquired snapshots, not the same maintained dynamic-BFS state contract.
- **Terrace** — dynamic graph storage and analytics, but not a public RisGraph-like exact incremental-BFS-maintenance contract.
- **LiveGraph, GraphOne, STINGER and LLAMA via GFE** — appropriate for separately labelled structural-update or snapshot-kernel experiments, not the main answer-ready incremental BFS table.

Pinned revisions and eligibility are recorded in [`benchmarks/external-baseline-manifest.json`](../benchmarks/external-baseline-manifest.json).

## Claim rules

1. External repositories are identified by immutable revisions or immutable version-to-revision mappings.
2. Dataset checksums, derivation rules, root policy, batch boundaries, timing envelope, thread settings, compiler/environment and retained artifacts accompany accepted results.
3. Correctness and workload nontriviality must pass before timing is accepted.
4. Update-only, snapshot-query and answer-ready maintenance timing are not mixed.
5. Same-machine paired ratios may be reported within one campaign; separate hosted runners are not combined into a cross-system ranking.
6. Hosted-CI results are engineering evidence, not universal or publication-grade superiority claims.
