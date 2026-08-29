# External dynamic baselines

VeloGraphX uses external systems only when the comparison can be pinned, rebuilt, and matched on graph/update semantics. The purpose of this document is to prevent broad or unfair speedup claims.

## Accepted dynamic-algorithm baseline: RisGraph

RisGraph is the open-source implementation of the SIGMOD 2021 paper *RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s*. The benchmark contract pins commit `4e77f774d4aa7cd0bf3011e713496573b70c91ab` and its Abseil submodule at `f1dad1e9b277066d676034d8f2a982b9e64310de`.

The accepted comparison scope is **directed, unweighted dynamic BFS with sliding-window batched updates**. Both systems receive the same native-endian binary edge stream, root, initial imported fraction, and batch size. Each batch inserts the next stream edges and removes the corresponding oldest edges. The timed maintenance envelope includes graph mutation and the work required to restore the BFS answer.

### Validated hosted-CI result

Evidence run: GitHub Actions `33267957508`, VeloGraphX head `7f38a5a5ab8373de9cbf29a9cbf679fbf33f90a5`.

The deterministic stream used seed `8675309`, 4,096 vertices, 32,768 unique directed edges, a 50% initial import, root 0, and 256-edge batches. The campaign executed 64 sliding-window batches on the same Ubuntu 24.04 hosted runner.

| System | Mean answer-ready batch time |
| --- | ---: |
| VeloGraphX | **920.625 µs** |
| RisGraph | **125.598 µs** |

On this workload RisGraph was approximately **7.33x faster** (`920.625 / 125.598`). This is a result in RisGraph's favor, and it is retained because the benchmark is semantically matched rather than selected to favor VeloGraphX.

Correctness was required before the timing ratio was accepted:

- VeloGraphX's maintained BFS distances exactly matched a separately rebuilt BFS on the final graph.
- RisGraph's maintained final BFS layer histogram exactly matched RisGraph's own post-stream full rebuild.
- The final layer histogram was identical across the two systems: `[1, 3, 9, 34, 127, 455, 1295, 1572, 474, 54, 3, 1, 1]`.

The comparison intentionally preserves an implementation asymmetry: VeloGraphX currently recomputes BFS whenever a batch contains deletions, while RisGraph implements incremental deletion repair. The result therefore measures **current end-to-end answer-ready maintenance under the same update semantics**; it must not be interpreted as an isolated storage, insertion, or BFS-kernel comparison. It also identifies deletion-aware BFS repair as a concrete VeloGraphX performance gap.

The immutable RisGraph source needed two build-only compatibility adaptations on the current Ubuntu runner: an explicit `<limits>` include in its pinned 2020 Abseil dependency, and replacement of the removed `tbb::task_scheduler_init` thread-limiting API with oneTBB `tbb::global_control`. The exact patches are retained with the workflow artifact. No graph algorithm, update policy, or benchmark logic was changed.

Artifact: `velographx-risgraph-dynamic-bfs-baseline`, artifact ID `9719227211`. Evidence workflow: `.github/workflows/risgraph-baseline.yml`.

## Screened but not used for the dynamic-BFS speedup table: Teseo

Teseo is the system from the VLDB 2021 paper *Teseo and the Analysis of Structural Dynamic Graphs*. The screened public revision is `2c37c2831c4d2acaaa838a86e1318363ce68c45b`. Its public repository is GPLv3, supports transactional structural updates, and exposes graph scans/Graphalytics-style kernels. It is a credible dynamic graph storage system, but its public API does not provide the same incremental-BFS maintenance contract as RisGraph and VeloGraphX.

Teseo is therefore **not** used as a second dynamic-BFS speedup baseline. A future storage-only comparison may use it if the experiment is explicitly scoped to equivalent structural operations such as insert/delete/scan, with no implication that the systems provide the same incremental algorithm semantics.

## Claim rules

1. External repositories must be identified by immutable commit SHA or release artifact.
2. Build commands, compiler/toolchain, dataset identity, update stream, thread count, and environment must be retained with the result.
3. Correctness or semantic equivalence must be checked before performance numbers are accepted.
4. Timing envelopes must describe what each system includes; update-only and answer-ready measurements must not be mixed.
5. Hosted-CI results are engineering evidence, not universal or publication-grade superiority claims.
6. A system that exposes different semantics may be discussed as a related structural baseline, but it must not be placed in the same speedup table.
