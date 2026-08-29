# 100M+ canonicalization A/B evidence

This document records a hosted-CI engineering A/B for reducing repeated canonical CSR rebuild cost on large dynamic graphs. It is **not** presented as a universal or publication-grade performance claim.

## Question

The hardened steady-state campaign showed that the conservative `1.25x` owned-storage cap kept memory bounded on com-Orkut, but forced a full validated O(E) CSR snapshot about every four epochs. Full canonicalization therefore dominated the maintenance path.

Two changes were evaluated together on the same code head:

1. **Cheaper compact-source traversal.** `consolidate_to_csr_snapshot()` now uses zero-copy `compact_neighbors()` spans when the source has no live deltas, avoiding one temporary adjacency-vector materialization/copy per vertex before rebuilding the fresh CSR.
2. **Scale-aware bounded storage envelope.** The general default remains `1.25x`. For graphs with at least 100M directed arcs, `scale_aware_consolidation_policy()` offers a `1.50x` hard storage cap while retaining the `1.25x` latency threshold and the existing latency persistence/cooldown controller.

The wider cap does not remove the hard bound: storage-triggered consolidation still bypasses cooldown immediately once the configured cap is reached.

## Reproducibility

Workflow: `Canonicalization A/B Evidence`

Run: `33265264254`

Evidence implementation head: `f94e7864dbf004de6737b9b8ca6138f6ae9fa0e0`

Dataset: canonical SNAP `com-Orkut`

- 3,072,441 distinct vertices
- 117,185,083 undirected edges
- 234,370,166 directed arcs inside `DynamicGraph`
- source archive SHA-256: `f73e33fb685f411a10c952f2ba3ea788380b91a17bc636e38da1a23f6c6b2bc6`
- 60 mutation/maintenance epochs
- 65,536 selected rows per epoch
- 8,192 sampled neighbor probes
- exact edge-count preservation and consolidation digest gates enabled

Both A and B completed with `all_correct=true` and preserved exactly 234,370,166 directed arcs.

## Result

| Metric | Conservative 1.25x | Bounded large-graph 1.50x | B / A |
| --- | ---: | ---: | ---: |
| Consolidations | 15 | **6** | **0.400x** |
| Consolidation interval | every 4 epochs | **every 9 epochs** | — |
| Total consolidation time | 386.573 s | **156.128 s** | **0.404x** |
| Consolidation share of maintenance path | 94.09% | **85.52%** | — |
| Maintenance-amortized throughput | 19,135 ops/s | **43,062 ops/s** | **2.250x** |
| Owned-storage high-water | 1.306x | **1.529x** | — |
| Process peak RSS | 7,517,892 KiB | **8,016,740 KiB** | **1.066x** |
| Sampled latency high-water | 3.474x | **2.174x** | — |

The scale-aware policy reduced consolidation frequency by **60%**, total measured consolidation time by **59.6%**, and increased maintenance-amortized throughput by **2.25x** on this single hosted Orkut execution. The explicit trade-off was a larger bounded owned-storage envelope and about **6.6% higher process peak RSS** on this runner.

The fresh conservative run also used the compact-source snapshot traversal. Compared with the immediately preceding hardened Orkut run on the older snapshot traversal (15 consolidations, about 431.9 s total consolidation time), the same 15-cutover conservative policy measured 386.6 s here. Because those are separate hosted executions, this approximately 10% difference is treated as directional engineering evidence rather than a controlled isolated microbenchmark of the traversal change.

## Interpretation

The accepted policy is intentionally tiered:

- **General graphs:** retain the conservative `1.25x` owned-storage cap.
- **100M+ directed-arc graphs:** the evidence-backed helper may use a `1.50x` hard cap to amortize O(E) rebuilds when the additional memory envelope is acceptable.
- **Latency path:** remains at `1.25x`, requires at least `1.10x` patch growth, five consecutive qualifying samples after cooldown, and a ten-epoch cooldown between latency-driven cutovers.
- **Storage path:** remains a hard trigger and bypasses cooldown at the configured storage cap.

The result does **not** show that 1.50x is universally optimal. It shows that on the retained com-Orkut workload, the previous 1.25x cap was too aggressive for a 100M+-class graph because the cost of repeated full canonicalization overwhelmed the maintenance path.

## Remaining optimization target

Even after the frequency reduction, consolidation still accounts for about **85.5%** of measured maintenance time in the bounded Orkut run. The next storage-systems step is therefore structural rather than threshold tuning: absorb compact row patches into selected CSR segments in place, or otherwise build/cut over canonical segments incrementally, so maintenance does not require repeated whole-graph reconstruction.
