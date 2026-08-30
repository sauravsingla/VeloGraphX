# External dynamic baselines

VeloGraphX uses external systems only when a comparison can be pinned, rebuilt or installed at an immutable version, matched on graph/update semantics, and guarded by correctness checks. The purpose of this document is to make the comparison boundary explicit and prevent broad or unfair speedup claims.

The machine-readable eligibility record is [`benchmarks/external-baseline-manifest.json`](../benchmarks/external-baseline-manifest.json).

## Comparison classes

External systems are separated into two classes:

1. **Same-semantics dynamic-BFS baselines** receive the same directed graph, source-order update stream, root, initial window, and batch boundaries, and must restore an exact BFS answer after every batch. These systems may appear in the dynamic-BFS latency evidence.
2. **Secondary dynamic-graph systems** support structural updates and graph analytics but do not expose the same maintained-BFS contract in their public artifact. They may be evaluated separately, but must not be mixed into the same latency table.

At present, the accepted same-semantics baselines are **RisGraph** and **NetworKit DynBFS**. Teseo, Aspen, Terrace, LiveGraph, GraphOne, and STINGER were screened as serious dynamic-graph systems but are not treated as equivalent incremental-BFS-maintenance baselines.

## Shared web-Google contract

The validated public workload uses checksum-pinned SNAP `web-Google`:

- `875,713` vertices and `5,105,039` directed edges;
- source SHA-256 `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1`;
- sparse SNAP IDs deterministically relabeled by ascending original ID to `0..875712`, without edge reordering;
- normalized stream SHA-256 `f451add10bfea6cdae5b7030410e0e93acbf5c4fc5f0821738b9863f0e9c6496`;
- 99% source-order initial import (`5,053,988` edges);
- 4,096-edge sliding-window batches, producing 13 update batches;
- each batch inserts the next source-order edges and deletes the equally sized oldest source-order edges;
- root selected deterministically as the maximum-out-degree vertex in the imported prefix, with smallest dense ID as tie-break;
- selected root `481807`, imported-prefix out-degree `456`;
- final exact BFS reaches `588,118` vertices.

The answer-ready timing envelope includes graph mutation and the work required to restore the BFS answer. Independent full-BFS verification is excluded from the timed region.

## RisGraph

RisGraph is the open-source implementation of the SIGMOD 2021 paper *RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s*. The benchmark pins commit `4e77f774d4aa7cd0bf3011e713496573b70c91ab` and its Abseil submodule at `f1dad1e9b277066d676034d8f2a982b9e64310de`.

### Validated hosted-CI result

Evidence run: GitHub Actions `33286241439`, VeloGraphX head `e3f6c21133a43a2c6826709c336e56b846c98252`. Artifact: `velographx-external-risgraph-web-google`, artifact ID `9724535579`.

| System / policy | Mean answer-ready batch time |
| --- | ---: |
| VeloGraphX deletion-aware repair | **59.658 ms** |
| VeloGraphX legacy full recomputation | **118.039 ms** |
| RisGraph | **31.333 ms** |

The deletion-aware VeloGraphX path is **1.98x faster than the former VeloGraphX deletion policy** on this workload. It processed `15,065` deletion candidates and `863,977` affected-vertex instances across 13 batches and required **0/13 full-recompute safety fallbacks**.

RisGraph remains approximately **1.90x faster** than the measured VeloGraphX path (`59.658 / 31.333`). The result therefore shows a substantial VeloGraphX internal improvement while also preserving the external performance gap; it must not be presented as VeloGraphX outperforming RisGraph.

Correctness was required before the timing ratio was accepted:

- VeloGraphX's maintained BFS exactly matched an independent full BFS.
- The reconstructed legacy VeloGraphX policy exactly matched an independent full BFS.
- RisGraph's maintained final BFS layer histogram exactly matched its own post-stream full rebuild.
- The final BFS layer histogram was identical across VeloGraphX and RisGraph.
- The evidence gate requires more than 1,000 reachable vertices to exclude degenerate roots.

The VeloGraphX deletion policy performs conservative pre-batch shortest-path-DAG closure invalidation, boundary reconstruction through reverse adjacency, final-state insertion relaxation, and a 35% affected-region fallback to full recomputation. Dependency discovery occurs before graph mutation so deleted shortest-path edges and multi-parent deletion interactions remain visible to the invalidation analysis.

### Build compatibility boundary

The immutable RisGraph source requires compatibility adaptations on current Ubuntu. Its legacy `FindTBB.cmake` expects a header removed from modern oneTBB; `tbb::task_scheduler_init` is replaced with oneTBB `global_control` for initialization-time thread limiting; and its pinned Abseil dependency needs an explicit `<limits>` include with modern GCC. The compatibility manifest records `algorithm_implementation_modified=false`; no graph algorithm, update policy, or benchmark logic is changed.

Retained compatibility identifiers include RisGraph patch SHA-256 `9f134bf4fd6591c2798aa4ba71ad96e951cfca671aebf9c8106abdc20eb88a26` and Abseil patch SHA-256 `238f1d81ab0bcc906a1263d7e043f187fef949c61cc6e140f8908e94d9ddd791`.

Evidence workflow: `.github/workflows/external-risgraph-baseline.yml`.

## NetworKit DynBFS

NetworKit provides a dedicated `DynBFS` implementation that handles edge additions and removals and exposes batched dynamic updates. The VeloGraphX benchmark pins **NetworKit 11.2.1**, corresponding to Git commit `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`.

The comparison uses exactly the same normalized `web-Google` stream, root, 99% initial import, 4,096-edge batches, and sliding-window update semantics described above. After **every batch**, NetworKit's maintained distance vector is checked against a freshly recomputed NetworKit BFS. The final BFS layer histogram is also required to match VeloGraphX exactly.

### Validated hosted-CI result

Evidence run: GitHub Actions `33289387036`, VeloGraphX head `287939734fc65220ba5177a6c2c3f74e29cb2487`. Artifact: `velographx-external-networkit-web-google`, artifact ID `9725483956`.

| System | Mean answer-ready batch time |
| --- | ---: |
| VeloGraphX deletion-aware repair | **41.682 ms** |
| NetworKit 11.2.1 `DynBFS` | **46.933 ms** |

In this single hosted-CI execution, VeloGraphX's measured answer-ready latency was approximately **11.2% lower** than NetworKit's (`41.682 / 46.933 = 0.888`). Equivalently, NetworKit took approximately **1.13x** the VeloGraphX time on this run.

This result has a stricter interpretation boundary than the native RisGraph comparison. NetworKit is invoked through its Python bindings. Its timed envelope includes Python-level graph mutation calls plus native `DynBFS` maintenance, while VeloGraphX executes through a native C++ harness. The result is therefore retained as **same-semantics engineering evidence**, not as a publication-grade language-neutral superiority claim. A future dedicated-hardware campaign should use a native C++ NetworKit harness before promoting the ratio to a research-level performance conclusion.

Correctness gates passed before the result was retained:

- NetworKit `DynBFS` matched a fresh full NetworKit BFS after **all 13/13 batches**.
- VeloGraphX matched its independent full BFS reference.
- Both systems used the same root, initial edge window, update boundaries, and final edge count.
- The final BFS layer histogram was identical across NetworKit and VeloGraphX.
- Both systems reached exactly `588,118` vertices in the final graph.

The NetworKit timed total decomposed into approximately **133.492 ms** of Python-level graph mutation and **476.641 ms** of dynamic-maintenance work across all 13 batches. Full-BFS correctness recomputation was outside the timed region.

Evidence workflow: `.github/workflows/external-networkit-baseline.yml`. Runner: `tools/run_networkit_dynamic_bfs.py`.

## Why the RisGraph and NetworKit numbers are not a league table

The RisGraph and NetworKit campaigns were separate GitHub Actions executions on hosted runners. Their VeloGraphX measurements differ (`59.658 ms` and `41.682 ms`) because hosted CI is noisy and machine allocation is not controlled. Therefore **RisGraph and NetworKit must not be ranked against each other by combining these two runs**. Only the same-run pairwise ratio within each campaign is meaningful as hosted-CI engineering evidence.

Publication-grade comparison requires repeated executions of all systems on the same dedicated machine, with controlled pinning, warm-up, repetitions, statistics, and identical native timing envelopes.

## Serious systems screened but excluded from the same-semantics table

### Teseo / GFE Driver

Teseo is the VLDB 2021 system *Teseo and the Analysis of Structural Dynamic Graphs*. The screened Teseo revision is `2c37c2831c4d2acaaa838a86e1318363ce68c45b`; the public GFE evaluation driver is pinned at `9cbb186c9b06f6e214ba0102beba2ec3080f8b95`.

GFE is a strong external evaluation framework and supports Teseo, LLAMA, GraphOne, STINGER, and LiveGraph. However, its public contract separates structural update experiments from Graphalytics kernel execution. It does not restore an incrementally maintained BFS answer after each VeloGraphX-style update batch. Teseo is therefore suitable for a **separately labelled structural-update or updated-snapshot BFS comparison**, not the main incremental-BFS latency table.

### Aspen

Aspen is the graph-streaming system from *Low-Latency Graph Streaming Using Compressed Purely-Functional Trees*. The screened public artifact is pinned at `ecc3193da05aef3b4e5f5de7cab77b215c0b8211`. It supports batched graph updates and BFS over acquired graph snapshots, but the public artifact does not expose RisGraph-like incremental BFS-state maintenance. Its build also relies on legacy Cilk Plus tooling. Aspen is retained as a credible secondary graph-streaming baseline rather than forced into a mismatched latency comparison.

### Terrace

Terrace is a serious dynamic graph-storage system and includes graph analytics, but its public contract similarly evaluates algorithms over dynamically updated graph state rather than maintaining exact BFS state with the same per-batch semantics. Its OpenCilk/Tapir-oriented build requirements also make a hosted-CI comparison materially different from the current VeloGraphX/RisGraph setup. It is screened but excluded from the same-semantics latency table.

### LiveGraph, GraphOne, STINGER, and LLAMA

These systems are supported by the GFE Driver and are relevant for structural dynamic-graph evaluation. They remain candidates for a future **separate** experiment on insertion/deletion throughput, memory behavior, or post-update Graphalytics kernels. Mixing those measurements with answer-ready incremental BFS latency would conflate different contracts.

## Historical synthetic RisGraph result

The earlier deterministic matched stream used seed `8675309`, 4,096 vertices, 32,768 unique directed edges, a 50% initial import, root 0, and 256-edge batches. Evidence run `33267957508` measured VeloGraphX at `920.625 µs` and RisGraph at `125.598 µs`, making RisGraph about **7.33x faster**. At that time VeloGraphX still recomputed the entire BFS after deletion-containing batches. The result is retained as historical evidence of the bottleneck that motivated deletion-aware repair; the public `web-Google` result is the current comparison.

## Claim rules

1. External repositories must be identified by immutable commit SHA or immutable release/version mapping.
2. Dataset identity, checksums, update stream, root, batch boundaries, timing envelope, thread configuration, compiler/toolchain, and environment must be retained with the result.
3. Correctness or semantic equivalence must pass before any performance number is accepted.
4. Update-only timing, snapshot-query timing, and answer-ready incremental-maintenance timing must never be mixed in one speedup table.
5. Results from different hosted runners must not be combined into a cross-system ranking.
6. Hosted-CI results are engineering evidence, not universal or publication-grade superiority claims.
7. Systems with different public semantics may be evaluated as secondary structural or snapshot baselines, but their different contract must be stated explicitly.
