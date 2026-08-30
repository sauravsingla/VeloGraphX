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

NetworKit is pinned to **11.2.1**, Git commit `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`. The workflow initializes and records its pinned submodules, builds the C++ core from source, compiles a native VeloGraphX comparison harness against `NetworKit::DynBFS`, and runs VeloGraphX and NetworKit sequentially on the **same GitHub-hosted runner**.

The timed envelope for both systems includes graph mutation plus restoration of the dynamic BFS answer. Fresh full-BFS verification is outside the timed region. `OMP_NUM_THREADS=1` is applied to the paired campaign. Each dataset is executed **five times**, and every repetition must satisfy:

- NetworKit `DynBFS` exactly matches a fresh full NetworKit BFS after every batch;
- VeloGraphX matches its independent full-BFS reference;
- root, initial/final edge counts, batch boundaries and final layer histogram agree across systems; and
- final reachability clears a dataset-specific nontriviality gate.

Accepted evidence run: **GitHub Actions `33291065285`**, VeloGraphX head `2e2d84f549a147dd52b7be7cb44709b2ffe0046c`, artifact **`9726044346`**, artifact SHA-256 `92146c420f47c38aa4e5992b3c7b845c94acbb1089334381c6eb195d522fb26b`.

### web-Google

Source SHA-256: `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1`. Normalized stream SHA-256: `f451add10bfea6cdae5b7030410e0e93acbf5c4fc5f0821738b9863f0e9c6496`.

- 875,713 vertices / 5,105,039 directed edges
- 99% initial import = 5,053,988 edges
- batch size 4,096 / 13 batches
- deterministic root 481807
- final reachable vertices 588,118; acceptance floor 100,000

| System | Mean batch | Median batch | Std. dev. |
| --- | ---: | ---: | ---: |
| VeloGraphX | **57.202 ms** | 56.814 ms | 1.473 ms |
| NetworKit 11.2.1 `DynBFS` | **43.742 ms** | 43.833 ms | 0.387 ms |

Five VeloGraphX samples were `59.653, 57.125, 56.730, 55.689, 56.814 ms`; NetworKit samples were `44.195, 43.595, 43.167, 43.920, 43.833 ms`. The mean paired VeloGraphX/NetworKit ratio is **1.308x** and the median paired ratio is **1.310x**. Thus **NetworKit is about 1.31x faster** on this native same-machine hosted-CI workload.

### ca-GrQc

The checksum-pinned source has 5,242 vertices and 28,980 rows. The provenance gate explicitly verifies 12 self-loops plus 14,484 unique non-loop undirected relationships represented reciprocally. Self-loops are removed, reciprocal representation is verified, vertices are dense-relabelled deterministically, and each unique relationship is emitted once in each direction.

Source SHA-256: `513efa8bb5c6d3d739797ca028d4a26a7df6bc20adcf3e722e18d1bcdb0e62d5`. Derived symmetric-stream SHA-256: `b6e16b41991049365670ac6407055034d2e29e21c8b000b9ecb0ff0ac8964192`.

- 5,242 vertices / 28,968 derived directed edges
- 75% initial import = 21,726 edges
- batch size 256 / 29 batches
- sliding-window-aware deterministic root 4282
- root out-degree: 79 in the initial window and 69 in the final window
- final reachable vertices 3,119; acceptance floor 1,000

| System | Mean batch | Median batch | Std. dev. |
| --- | ---: | ---: | ---: |
| VeloGraphX | **0.3952 ms** | 0.3944 ms | 0.0018 ms |
| NetworKit 11.2.1 `DynBFS` | **0.08127 ms** | 0.08078 ms | 0.00219 ms |

Five VeloGraphX samples were `0.3944, 0.3938, 0.3958, 0.3938, 0.3981 ms`; NetworKit samples were `0.07893, 0.08030, 0.08154, 0.08078, 0.08479 ms`. The mean paired VeloGraphX/NetworKit ratio is **4.865x** and the median paired ratio is **4.875x**. Thus **NetworKit is about 4.86x faster** on this smaller collaboration-network workload.

The earlier ca-GrQc attempt that ended with only one reachable vertex is intentionally excluded; the workflow was hardened to select a root robust across the initial and final sliding windows and to fail when final reachability is below 1,000 vertices.

## Interpretation

The native C++ campaign supersedes the earlier Python-binding NetworKit timing as the primary NetworKit evidence. The earlier Python result remains historical only; its timing included Python-level mutation overhead and should not be used for performance claims.

The new campaign materially improves fairness: both implementations are native C++, pinned, executed on the same runner, repeated five times, and checked for exact answers and nontrivial reachability. It also shows that **VeloGraphX does not currently outperform NetworKit DynBFS on either accepted workload**.

These remain **hosted-CI engineering measurements**, not publication-grade universal conclusions. The runner is virtualized and uncontrolled, only one thread is used, there are two graph families, and the RisGraph campaign is not on the same machine as this NetworKit campaign. Publication-level conclusions require dedicated hardware, controlled CPU placement/frequency, more graph families and roots/update regimes, more repetitions, multicore scaling, and a same-machine campaign containing all native competitors.

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
