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

The timed envelope includes graph mutation plus restoration of the dynamic BFS answer. Fresh full-BFS verification is outside the timed region. `OMP_NUM_THREADS=1` and `OMP_PROC_BIND=true` are applied. Each dataset runs **five paired repetitions**; every repetition must match exact full BFS and pass a nontrivial final-reachability gate.

Accepted evidence run: **GitHub Actions `33301190847`**, VeloGraphX head `3c1f7448897ffdca227a261c61bd49751e42fa5f`, source optimization commit `7a4083656cc2dfe67903efe7bdc7d822a337bd3e`, artifact **`9729078197`**, artifact SHA-256 `c031a2316a91c7958ce6a4e0a03e762808a777688fb05f0db3716f449875ef7c`.

### web-Google

Source SHA-256: `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1`. Normalized stream SHA-256: `f451add10bfea6cdae5b7030410e0e93acbf5c4fc5f0821738b9863f0e9c6496`.

- 875,713 vertices / 5,105,039 directed edges
- 99% initial import = 5,053,988 edges
- batch size 4,096 / 13 batches
- deterministic root 481807
- final reachable vertices 588,118; acceptance floor 100,000

| System | Mean batch | Median batch | Std. dev. |
| --- | ---: | ---: | ---: |
| VeloGraphX | **31.512 ms** | 31.436 ms | 0.827 ms |
| NetworKit 11.2.1 `DynBFS` | **41.293 ms** | 41.143 ms | 1.355 ms |

VeloGraphX samples were `31.311, 32.687, 31.436, 31.737, 30.389 ms`; NetworKit samples were `42.338, 39.819, 40.191, 41.143, 42.977 ms`. The mean paired VeloGraphX/NetworKit ratio is **0.7642x** and the median paired ratio is **0.7714x**. On this hosted runner, VeloGraphX therefore used about **23.6% less answer-ready batch time** than NetworKit for this workload.

### ca-GrQc

The checksum-pinned source has 5,242 vertices and 28,980 rows. The provenance gate verifies 12 self-loops plus 14,484 unique non-loop undirected relationships represented reciprocally. Self-loops are removed and each verified relationship is emitted once in each direction.

Source SHA-256: `513efa8bb5c6d3d739797ca028d4a26a7df6bc20adcf3e722e18d1bcdb0e62d5`. Derived symmetric-stream SHA-256: `b6e16b41991049365670ac6407055034d2e29e21c8b000b9ecb0ff0ac8964192`.

- 5,242 vertices / 28,968 derived directed edges
- 75% initial import = 21,726 edges
- batch size 256 / 29 batches
- sliding-window-aware deterministic root 4282
- root out-degree: 79 initially and 69 in the final window
- final reachable vertices 3,119; acceptance floor 1,000

| System | Mean batch | Median batch | Std. dev. |
| --- | ---: | ---: | ---: |
| VeloGraphX | **0.1110 ms** | 0.1110 ms | 0.00091 ms |
| NetworKit 11.2.1 `DynBFS` | **0.08080 ms** | 0.08051 ms | 0.00065 ms |

VeloGraphX samples were `0.109765, 0.112258, 0.111030, 0.111380, 0.110741 ms`; NetworKit samples were `0.081171, 0.080466, 0.081741, 0.080513, 0.080115 ms`. The mean paired VeloGraphX/NetworKit ratio is **1.3743x** and the median paired ratio is **1.3823x**. NetworKit remains faster on this small workload, but the gap has narrowed from several-fold to about 37% on this campaign.

The focused ca-GrQc optimization campaign preserved BFS semantics while attacking fixed overhead. The effective-neighbor traversal is allocation-free and exactly merges compact CSR, row patches and live deltas; BFS workspaces reuse repair heap storage and flat generation-stamped update-key tables; packed-delta mutation avoids duplicate forward lookup; and percentage-triggered automatic global delta maintenance now requires at least **16,384 live delta entries**. A controlled same-run 4,096-vs-16,384 maintenance-floor A/B measured ca-GrQc at **272.045 µs vs 115.771 µs** (candidate/base `0.4256`) while web-Google slightly improved from **35.542 ms to 35.303 ms**. All A/B executions were exact and all 27 candidate tests passed. The accepted maintenance change is commit `7a4083656cc2dfe67903efe7bdc7d822a337bd3e`; A/B artifact `9728969220`, SHA-256 `ee50b010ef5c52e9402465968499d22d7bdaa77d6da3f6268ab928f7f2795b68`.

Relative to the earlier clean ca-GrQc VeloGraphX mean of **0.3884 ms**, the final canonical mean of **0.1110 ms** is about **71% lower**. Relative to the immediately preceding `0.3134 ms` campaign, it is about **65% lower**. The frozen workload, roots, batch sizes, thread settings, competitor revision and exactness gates were unchanged.

### Supplementary exact multi-root evidence

A separate workflow broadens root coverage without changing the canonical external comparison. Candidate roots are selected deterministically using graph support and final-graph reachability only; benchmark timing is not used for root selection. Each selected root is then run through the same exact VeloGraphX dynamic workload with independent full-BFS validation.

Evidence run: GitHub Actions `33301366020`, artifact `9729058306`, artifact SHA-256 `cab9eec6489c8b9c65d57b2b781a5241a1954a5d5e99e94577c9ce0c737cccfa`.

| Dataset / root | Final reachable | Mean batch |
| --- | ---: | ---: |
| `ca-GrQc` / 4282 | 3,119 | **103.546 µs** |
| `ca-GrQc` / 2465 | 3,119 | **105.039 µs** |
| `ca-GrQc` / 1974 | 3,119 | **108.345 µs** |
| `web-Google` / 481807 | 588,118 | **30.699 ms** |
| `web-Google` / 771121 | 588,118 | **30.986 ms** |
| `web-Google` / 391806 | 588,283 | **22.691 ms** |

All six roots passed exact incremental-vs-full validation and the dataset-specific nontrivial reachability gate. The mean of the three ca-GrQc root means is **105.643 µs**; the mean of the three web-Google root means is **28.125 ms**. These numbers demonstrate root robustness of VeloGraphX only; they are not a multi-root NetworKit comparison.

## Interpretation

The final optimized campaign supersedes the previous native NetworKit campaign as the primary NetworKit evidence. It shows a strong same-run web-Google result and reduces ca-GrQc from the principal several-fold gap to a much smaller remaining gap while preserving exactness. The separate multi-root run also removes the earlier single-root-only limitation for VeloGraphX correctness/performance evidence on these two datasets.

These are **hosted-CI engineering measurements**, not publication-grade universal conclusions. Pairwise same-run ratios are more informative than absolute comparisons across different hosted runners. Publication-level conclusions still require dedicated hardware, additional graph families/update regimes, multi-root same-machine competitor comparisons, multicore scaling, hardware counters, and independent reproduction.

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
