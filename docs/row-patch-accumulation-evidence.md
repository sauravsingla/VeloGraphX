# Row-patch accumulation and CSR consolidation evidence

This campaign measures what happens when VeloGraphX runs for many mutation/compaction cycles after adopting row-local compact patches, and whether rebuilding a validated canonical segmented-CSR snapshot recovers memory and neighbor-access locality.

## Consolidation model

Row-local compaction is intentionally cheap for sparse updates: touched rows become compact patches while untouched rows remain in the original 65,536-vertex segmented CSR. Over a long-running workload, however, those patches accumulate.

`consolidate_to_csr_snapshot()` rebuilds the current logical graph into a fresh segmented CSR plus transpose. It does **not** mutate the source graph. Callers can validate the snapshot and perform an application-level cutover only after correctness checks pass. The returned snapshot begins with empty row-patch and delta layers.

Consolidation is therefore an explicit O(E) maintenance operation, not hidden work inside `DynamicGraph::apply()`.

## Real irregular datasets

The source inputs are checksum-pinned public graphs already represented in the repository's dataset provenance.

| Dataset | Direction | Vertices | Normalized edges | Source SHA-256 |
|---|---|---:|---:|---|
| `ca-GrQc` | undirected | 5,242 | 14,484 | `513efa8bb5c6d3d739797ca028d4a26a7df6bc20adcf3e722e18d1bcdb0e62d5` |
| `web-Google` | directed | 875,713 | 5,105,039 | `8c0f453f1eb1e24ad145e36e542b129083237e96e585abae768927bdb70167d1` |

`ca-GrQc` uses 64 selected source rows per cycle and five repetitions. `web-Google` uses 4,096 selected source rows per cycle and three repetitions. Each mutation replaces a known existing edge with a deterministic known-absent destination, after which row-local compaction is applied. The campaign records 5, 20 and 50 cycles.

## Correctness gate

Every repetition requires all of the following before it is accepted:

- identical full logical adjacency digest before and after CSR consolidation;
- identical deterministic sampled-neighborhood digest;
- identical directed edge count;
- a compact consolidated graph with no pending delta divergence.

All **24 final repetitions** passed: 15 on `ca-GrQc` and 9 on `web-Google`.

## ca-GrQc accumulation curve

Medians from the final workflow run:

| Cycles | Applied update operations | Owned storage / pristine CSR | Neighbor latency / pristine | Consolidated storage / accumulated | Post-consolidation / pre-consolidation latency | Consolidation time |
|---:|---:|---:|---:|---:|---:|---:|
| 5 | 638 | **1.213x** | **1.293x** | **0.824x** | **0.836x** | 2.671 ms |
| 20 | 2,542 | **1.587x** | **1.374x** | **0.630x** | **0.766x** | 2.996 ms |
| 50 | 6,284 | **1.927x** | **1.471x** | **0.519x** | **0.753x** | 3.096 ms |

The small irregular graph shows both memory growth and access-locality degradation as patches accumulate. At 20 cycles, consolidation removes roughly 37% of the accumulated owned storage and reduces sampled neighbor latency by roughly 23%. At 50 cycles, the fresh CSR uses roughly 52% of the accumulated owned storage and sampled neighbor latency falls by roughly 25%.

## web-Google accumulation curve

Medians from the 875,713-vertex / 5,105,039-edge directed workload:

| Cycles | Applied update operations | Owned storage / pristine CSR | Neighbor latency / pristine | Consolidated storage / accumulated | Post-consolidation / pre-consolidation latency | Consolidation time |
|---:|---:|---:|---:|---:|---:|---:|
| 5 | 34,688 | **1.070x** | **1.007x** | **0.935x** | **0.983x** | 247.933 ms |
| 20 | 138,098 | **1.175x** | **1.334x** | **0.851x** | **0.777x** | 341.048 ms |
| 50 | 345,612 | **1.332x** | **1.719x** | **0.751x** | **0.604x** | 345.007 ms |

The large directed graph exposes an important difference from the small graph: **neighbor-access degradation becomes the earlier warning signal**. At 20 cycles, owned storage has grown only about 17%, but sampled neighbor latency is already about 33% above the pristine CSR baseline. At 50 cycles, storage is about 33% above baseline while neighbor latency is about 72% above baseline. Consolidation brings the 50-cycle sampled latency down from 362.773 ns/probe to 219.042 ns/probe, close to the pristine 211.007 ns/probe median.

## Evidence-based maintenance signal

The two graphs do not support a storage-only trigger. `ca-GrQc` reaches substantial storage growth quickly, while `web-Google` shows substantial access degradation before storage reaches 1.5x.

The repository therefore provides an explicit `ConsolidationPolicy` helper with engineering defaults:

- consolidate when owned storage reaches **1.25x** the most recent canonical-CSR baseline; **or**
- consolidate when sampled neighbor latency reaches **1.25x** its canonical-CSR baseline.

The policy is deliberately an **OR**, and it only returns a signal. It does not run consolidation automatically inside the update path. Applications can schedule snapshot construction at an appropriate maintenance boundary, validate it, and then cut over.

The 1.25x defaults are an engineering policy derived from these two hosted-CI workloads, not a universal optimum. A deployment should calibrate them against its graph, mutation locality, latency objective and available maintenance window.

## Instrumentation note

An early ca-GrQc instrumentation run produced an invalid post-consolidation storage ratio because the benchmark sampled `storage_bytes()` after moving the consolidated graph into the result object. That storage measurement was discarded. The helper was corrected to sample storage before the move, and all numbers in this document come from corrected runs. The logical-digest correctness gate was unaffected by that instrumentation issue.

## Reproducibility and evidence boundary

Final evidence workflow: `Row Patch Accumulation Evidence`, run `33260929836`, source head `71ab7f7e28ce4efe9841a3243dc5e5e223266e2d`. Artifacts are retained for each dataset/cycle combination for 90 days.

These are hosted-CI engineering measurements. They establish reproducible behavior on one small undirected collaboration graph and one substantially larger directed web graph. They do not establish a universal consolidation threshold or publication-grade hardware performance. Stronger publication evidence should add controlled hardware, repeated larger graph families, alternative mutation-locality patterns, hot-vertex/skewed update distributions, steady-state cycles with repeated consolidation, and end-to-end application throughput including maintenance-window amortization.
