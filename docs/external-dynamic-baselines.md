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

Accepted evidence run: **GitHub Actions `33294575513`**, VeloGraphX head `2ebcd9107afeb977b41ad11be0ec4734ce767f63`, artifact **`9727073670`**, artifact SHA-256 `7c1e04ad40c8e4b94f980bd8e4b9c44c282120dfada040d31fb2eec467cc5bdd`.

### web-Google

Source SHA-256: `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1`. Normalized stream SHA-256: `f451add10bfea6cdae5b7030410e0e93acbf5c4fc5f0821738b9863f0e9c6496`.

- 875,713 vertices / 5,105,039 directed edges
- 99% initial import = 5,053,988 edges
- batch size 4,096 / 13 batches
- deterministic root 481807
- final reachable vertices 588,118; acceptance floor 100,000

| System | Mean batch | Median batch | Std. dev. |
| --- | ---: | ---: | ---: |
| VeloGraphX | **44.531 ms** | 44.303 ms | 1.426 ms |
| NetworKit 11.2.1 `DynBFS` | **43.607 ms** | 43.763 ms | 0.407 ms |

VeloGraphX samples were `46.039, 43.136, 44.303, 43.198, 45.978 ms`; NetworKit samples were `43.911, 43.256, 43.763, 44.011, 43.096 ms`. The mean paired VeloGraphX/NetworKit ratio is **1.021x** and the median paired ratio is **1.012x**. On this hosted runner, VeloGraphX is therefore within about **2.1% of NetworKit**.

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
| VeloGraphX | **0.3884 ms** | 0.3896 ms | 0.0030 ms |
| NetworKit 11.2.1 `DynBFS` | **0.08792 ms** | 0.08808 ms | 0.00043 ms |

VeloGraphX samples were `0.3896, 0.3919, 0.3896, 0.3860, 0.3846 ms`; NetworKit samples were `0.08835, 0.08808, 0.08766, 0.08732, 0.08821 ms`. The mean paired VeloGraphX/NetworKit ratio is **4.417x**. NetworKit remains materially faster on this small-graph workload.

The earlier degenerate ca-GrQc run is excluded; the workflow now selects a root robust across the initial and final sliding windows and rejects final reachability below 1,000 vertices.

## Interpretation

The clean optimized campaign supersedes the earlier native NetworKit campaign as the primary NetworKit evidence. It shows **near parity on web-Google**, while the smaller ca-GrQc workload remains a clear optimization gap.

These are **hosted-CI engineering measurements**, not publication-grade universal conclusions. Pairwise same-run ratios are more informative than absolute comparisons across different hosted runners. Publication-level conclusions require dedicated hardware, more graph families and roots/update regimes, multicore scaling, hardware counters, and a same-machine campaign containing all native competitors.

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
