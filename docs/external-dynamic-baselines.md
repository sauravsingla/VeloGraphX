# External dynamic baselines

VeloGraphX uses external systems only when the comparison can be pinned, rebuilt, and matched on graph/update semantics. The purpose of this document is to prevent broad or unfair speedup claims.

## Accepted dynamic-algorithm baseline: RisGraph

RisGraph is the open-source implementation of the SIGMOD 2021 paper *RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s*. The benchmark contract pins commit `4e77f774d4aa7cd0bf3011e713496573b70c91ab` and builds the repository from source.

The accepted comparison scope is **directed, unweighted dynamic BFS with sliding-window batched updates**. Both systems receive the same native-endian binary edge stream, root, initial imported fraction, and batch size. Each batch inserts the next stream edges and removes the corresponding oldest edges. The timed maintenance envelope includes graph mutation and the work required to restore the BFS answer.

The workflow additionally requires the final BFS layer histogram to agree exactly before any timing ratio is emitted. VeloGraphX also checks its final maintained distances against a separately recomputed BFS result.

This comparison intentionally preserves an implementation asymmetry: VeloGraphX currently recomputes BFS when a batch contains deletions, while RisGraph has an incremental deletion-repair path. The result therefore measures current end-to-end maintenance behavior under matched update semantics; it must not be interpreted as an isolated storage, insertion, or kernel comparison.

Evidence workflow: `.github/workflows/risgraph-baseline.yml`.

## Screened but not used for the dynamic-BFS speedup table: Teseo

Teseo is the system from the VLDB 2021 paper *Teseo and the Analysis of Structural Dynamic Graphs*. Its public repository is GPLv3, supports transactional structural updates, and exposes graph scans/Graphalytics-style kernels. It is a credible dynamic graph storage system, but its public API does not provide the same incremental-BFS maintenance contract as RisGraph and VeloGraphX.

Teseo is therefore **not** used as a second dynamic-BFS speedup baseline. A future storage-only comparison may use it if the experiment is explicitly scoped to equivalent structural operations (for example insert/delete/scan), with no implication that the systems provide the same incremental algorithm semantics.

## Claim rules

1. External repositories must be identified by immutable commit SHA or release artifact.
2. Build commands, compiler/toolchain, dataset identity, update stream, thread count, and environment must be retained with the result.
3. Correctness or semantic equivalence must be checked before performance numbers are accepted.
4. Timing envelopes must describe what each system includes; update-only and answer-ready measurements must not be mixed.
5. Hosted-CI results are engineering evidence, not universal or publication-grade superiority claims.
6. A system that exposes different semantics may be discussed as a related structural baseline, but it must not be placed in the same speedup table.
