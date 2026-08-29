# External dynamic-graph baselines

This document defines the admissible scope for external comparative evidence in VeloGraphX. The goal is to compare matched semantics, not merely similarly named systems.

## Accepted algorithm baseline: RisGraph

RisGraph is the open-source implementation of the SIGMOD 2021 system *RisGraph: A Real-Time Streaming System for Evolving Graphs to Support Sub-millisecond Per-update Analysis at Millions Ops/s*.

Pinned source:

- repository: `thu-pacman/RisGraph`
- commit: `4e77f774d4aa7cd0bf3011e713496573b70c91ab`
- license: Apache-2.0
- build mode: Release
- matched executable: `bfs_inc_batch`

The comparison uses RisGraph's own batched sliding-window semantics: load an initial edge prefix, then for each batch insert the next edges and incrementally update BFS, followed by deleting the oldest edges and incrementally updating BFS again. VeloGraphX replays that same two-phase sequence rather than combining insertions and deletions into a different batch contract.

The evidence workflow fixes the root, import fraction, batch size, deterministic edge stream, compiler family, runner, and thread count. Before any timing ratio is emitted, the workflow requires:

1. VeloGraphX incremental BFS to match a fresh VeloGraphX full recomputation after the final update.
2. VeloGraphX and RisGraph to report identical initial BFS layer counts.
3. VeloGraphX and RisGraph to report identical final BFS layer counts.
4. The exact RisGraph commit to match the declared immutable pin.

Five same-runner repetitions are retained. Reported timing is the median wall time for the complete sliding-window update phase. The workflow records raw outputs, compiler, CPU topology, workload checksum, competitor commit, and TBB commit.

This is hosted-CI engineering evidence. It must not be described as universal superiority, a reproduction of the paper's headline throughput, or a publication-grade hardware comparison.

## Evaluated but excluded from the incremental-algorithm table: Teseo

Teseo is the source code for *Teseo and the Analysis of Structural Dynamic Graphs* (PVLDB 2021). It is a credible structural dynamic-graph system with transactional insert/remove and adjacency-iteration APIs, and its repository provides a reproducible Linux build recipe.

Pinned source considered:

- repository: `cwida/teseo`
- commit: `2c37c2831c4d2acaaa838a86e1318363ce68c45b`
- license: GPL-3.0

Teseo is deliberately **not** included in the incremental-BFS speedup table because its public system interface is a dynamic transactional graph store; it does not expose the same incremental BFS-maintenance contract as VeloGraphX or RisGraph. Comparing VeloGraphX's update-plus-algorithm-repair latency directly with Teseo's structural update latency would be an apples-to-oranges result.

Teseo remains eligible for a future **storage-only** experiment if both systems are measured on the same undirected simple-graph semantics, identical update stream, identical transaction/batch boundaries, and an equivalent adjacency-scan/read workload. Such a result must be labeled as dynamic-storage evidence only.

## Fairness rules

- Compare algorithm maintenance only when both systems maintain the same result under the same update semantics.
- Keep structural storage comparisons separate from algorithm-maintenance comparisons.
- Use immutable source revisions and record all non-system dependency pins needed to build an older competitor.
- Do not alter competitor algorithms to improve or weaken performance. Compatibility-only build changes, if ever required, must be recorded as patches and retained with the artifact.
- Match thread count for the primary comparison. Multithread scaling is a separate experiment.
- Validate result equivalence before reporting a timing ratio.
- Preserve raw competitor output; do not compare only parsed headline numbers.
- Hosted runners are engineering evidence, not controlled publication hardware.
