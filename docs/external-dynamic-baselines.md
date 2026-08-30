# External dynamic baselines

VeloGraphX uses external systems only when the comparison can be pinned, rebuilt, and matched on graph/update semantics. The purpose of this document is to prevent broad or unfair speedup claims.

## Accepted dynamic-algorithm baseline: RisGraph

RisGraph is the open-source implementation of the SIGMOD 2021 paper *RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s*. The benchmark contract pins commit `4e77f774d4aa7cd0bf3011e713496573b70c91ab` and its Abseil submodule at `f1dad1e9b277066d676034d8f2a982b9e64310de`.

The accepted comparison scope is **directed, unweighted dynamic BFS with sliding-window batched updates**. Both systems receive the same source-order edge stream, root, initial imported fraction, and batch size. Each batch inserts the next stream edges and removes the corresponding oldest edges. The timed maintenance envelope includes graph mutation and the work required to restore the BFS answer.

### Current validated hosted-CI result: web-Google

Evidence run: GitHub Actions `33286241439`, VeloGraphX head `e3f6c21133a43a2c6826709c336e56b846c98252`. Artifact: `velographx-external-risgraph-web-google`, artifact ID `9724535579`.

The source is checksum-pinned SNAP `web-Google` (`875,713` vertices, `5,105,039` directed edges, SHA-256 `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1`). Because the original SNAP IDs are sparse, they are deterministically relabeled by ascending original ID into `0..875712`; edge order is not changed. The normalized stream SHA-256 is `f451add10bfea6cdae5b7030410e0e93acbf5c4fc5f0821738b9863f0e9c6496`.

The run imported 99% of the source-order stream (`5,053,988` edges), used 4,096-edge sliding-window batches, and executed 13 update batches. To avoid a trivial BFS tree, the root was selected deterministically as the maximum-out-degree vertex in the imported prefix, with the smallest dense ID as tie-break. The selected root was `481807`, with imported-prefix out-degree `456`, and the final BFS reached `588,118` vertices.

| System / policy | Mean answer-ready batch time |
| --- | ---: |
| VeloGraphX deletion-aware repair | **59.658 ms** |
| VeloGraphX legacy full recomputation | **118.039 ms** |
| RisGraph | **31.333 ms** |

The new VeloGraphX deletion-aware path is **1.98x faster than the former VeloGraphX deletion policy** on this workload. It processed `15,065` deletion candidates and `863,977` affected-vertex instances across the 13 batches and required **0/13 full-recompute safety fallbacks**. This establishes that deletion-aware repair materially improves VeloGraphX itself on a nontrivial public graph workload.

RisGraph remains approximately **1.90x faster** than the new VeloGraphX path (`59.658 / 31.333`). The engineering improvement therefore reduces the external gap but does not close it. This result must not be presented as VeloGraphX outperforming RisGraph.

Correctness was required before either timing ratio was accepted:

- VeloGraphX's deletion-aware maintained BFS exactly matched an independent full BFS.
- The reconstructed legacy VeloGraphX policy exactly matched an independent full BFS.
- RisGraph's maintained final BFS layer histogram exactly matched RisGraph's own post-stream full rebuild.
- The final BFS layer histogram was identical across VeloGraphX and RisGraph.
- The benchmark requires more than 1,000 reachable vertices so a degenerate root cannot enter the evidence table.

The VeloGraphX deletion policy used here performs conservative **pre-batch shortest-path-DAG closure invalidation**, boundary reconstruction through reverse adjacency, final-state insertion relaxation, and a 35% affected-region fallback to full recomputation. Dependency discovery occurs before graph mutation so deleted shortest-path edges and multi-parent deletion interactions cannot disappear from the invalidation analysis. The conservative closure can invalidate more vertices than a precise support-count algorithm; reducing that over-invalidation is the next obvious BFS optimization.

### Historical synthetic matched result

The earlier deterministic matched stream used seed `8675309`, 4,096 vertices, 32,768 unique directed edges, a 50% initial import, root 0, and 256-edge batches. Evidence run `33267957508` measured VeloGraphX at `920.625 µs` and RisGraph at `125.598 µs`, making RisGraph about **7.33x faster**. At that time VeloGraphX still recomputed the entire BFS after deletion-containing batches. The result is retained as historical evidence of the bottleneck that motivated the deletion-aware implementation; the public web-Google result above is the current comparison.

### Build compatibility boundary

The immutable RisGraph source requires build/runtime compatibility adaptations on current Ubuntu: its legacy `FindTBB.cmake` expects a header removed from modern oneTBB, `tbb::task_scheduler_init` was replaced with oneTBB `global_control` for initialization-time thread limiting, and its pinned Abseil dependency needs an explicit `<limits>` include with modern GCC. The compatibility manifest records `algorithm_implementation_modified=false`; no graph algorithm, update policy, or benchmark logic is changed.

Current retained compatibility identifiers include RisGraph patch SHA-256 `9f134bf4fd6591c2798aa4ba71ad96e951cfca671aebf9c8106abdc20eb88a26` and Abseil patch SHA-256 `238f1d81ab0bcc906a1263d7e043f187fef949c61cc6e140f8908e94d9ddd791`.

Evidence workflow: `.github/workflows/external-risgraph-baseline.yml`. The smaller synthetic compatibility workflow remains `.github/workflows/risgraph-baseline.yml`.

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
