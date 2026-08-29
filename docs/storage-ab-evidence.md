# Dynamic-storage A/B evidence

This campaign compares VeloGraphX dynamic storage with the implementation immediately preceding the segmented-storage upgrade.

## Compared designs

**Historical baseline.** Reconstructed from commit `22d05c6b54b9199c852062395b3d6536abca02d9`: `std::vector<std::vector<VertexId>>` compact rows plus per-vertex `std::unordered_set<VertexId>` insertion and deletion deltas. It does not maintain reverse adjacency.

**Current design.** A 65,536-vertex segmented CSR base, shared packed sorted delta arenas, explicit transposed CSR and synchronized reverse deltas. Sparse explicit compaction materializes only touched rows into compact row patches; untouched rows remain in the contiguous CSR. Automatic row compaction is deferred while the global delta ratio is below 1%, avoiding maintenance scans on sparse batches.

The historical implementation exists only inside the benchmark harness; production code is not switched between implementations.

## Workload

The hosted campaign creates deterministic directed degree-20 graphs. The 10M-edge case contains 500,000 vertices; the 100M-edge case contains 5,000,000 vertices. Each run applies a deterministic mixed batch equal to 0.1% of base edges, alternating deletions of known edges and insertions of known-absent edges.

Neighbor latency uses 8,192 deterministic probes after updates and before explicit compaction. A correctness gate requires identical sampled logical-neighborhood checksums and identical final directed edge counts. Linux RSS is sampled after releasing the generated input edge vector and requesting `malloc_trim(0)`. The 10M case uses three repetitions and medians; the 100M case is a single hosted execution.

## Evolution of the compaction design

The same benchmark has been retained across storage iterations so regressions are visible rather than hidden.

| Design | 10M update throughput | 10M neighbor latency | 10M compaction | 100M update throughput | 100M neighbor latency | 100M compaction |
|---|---:|---:|---:|---:|---:|---:|
| Initial segmented/packed | **1.887x** | **0.784x** | 11.313x | **1.332x** | **0.863x** | 16.159x |
| 65K dirty-segment compaction | **1.077x** | **0.793x** | 9.629x | **1.278x** | **0.794x** | 11.989x |
| **Row-local compact patches** | **1.069x** | **0.780x** | **1.657x** | **1.628x** | **0.783x** | **1.492x** |

Ratios are current / historical within each A/B job. Higher is better for update throughput; lower is better for latency and compaction time. Correctness passed in all retained runs.

The row-local result is from production commit `58d9cbebde09388b70e0d1c0d37a46cf3238ee87`, workflow run `33260091622`. The 10M and 100M jobs both passed the neighborhood-checksum and final-edge-count gates and uploaded retained artifacts.

### What changed

The earlier 65K-segment design rebuilt a full 65,536-row CSR segment whenever any row in that segment was compacted. On broadly distributed sparse updates, most large segments could therefore become dirty. The current design keeps the large CSR for locality but stores newly compacted touched rows in sparse row patches. Forward rows and reverse rows are patched independently, so explicit compaction work is proportional to touched rows rather than all rows in touched 65K segments.

A 64-row CSR-segment experiment was also measured and rejected. It reduced 100M compaction to about 6.10x historical but pushed update throughput below historical (about 0.92x) and made 100M neighbor access slightly slower than historical. The result showed that simply creating many tiny CSR segments trades away too much locality and metadata efficiency. The row-patch design keeps the useful large-CSR layout while localizing compaction.

### Improvement over the previous production compaction

Relative to the 65K dirty-segment implementation, the historical-normalized compaction ratio falls:

- **10M:** 9.629x → **1.657x**, about **82.8% lower**.
- **100M:** 11.989x → **1.492x**, about **87.6% lower**.

At 100M, update throughput also rises from 1.278x to **1.628x historical**, while neighbor materialization improves from 0.794x to **0.783x historical**. At 10M, update throughput is essentially unchanged within hosted-run variation (1.077x → **1.069x**) while compaction improves substantially.

Loaded RSS remains favorable at **0.967x historical at 10M** and **0.966x at 100M** in the final run. The row-patch layer is empty after initial bulk load, so it does not inflate the reported loaded-state comparison.

## Reverse-adjacency cost

For the regular degree-20 directed workload, reverse CSR contains the same number of arcs as forward CSR and therefore requires approximately the same compact base storage: about **42 MiB at 10M edges** and **420 MiB at 100M edges** in the original reverse-storage measurement.

This is an explicit functionality cost. It enables direct predecessor traversal for algorithms such as localized PageRank instead of global predecessor discovery.

## Interpretation

For this workload, the current row-patch design gives the strongest overall storage result measured so far: slightly lower loaded RSS than the historical layout, lower logical neighbor latency, higher update throughput at both tested scales, and explicit sparse compaction within roughly 1.5–1.7x of the historical row-local implementation despite also maintaining reverse adjacency.

The remaining trade-offs are still material. Bulk loading remains slower because the current layout sorts/deduplicates arcs and constructs the transpose. Row patches also add a second compact-row lookup after rows have been patched, so long-running mutation workloads should be evaluated for patch accumulation and periodic consolidation back into CSR.

## Evidence boundary

These are hosted-CI engineering measurements on synthetic regular graphs, not universal or publication-grade claims. The 10M case is repeated; the 100M case is a single hosted execution. Runner CPU models can differ between workflow jobs, so comparisons are interpreted within each A/B job rather than as direct 10M-versus-100M scaling measurements.

Publication-quality evidence should add irregular real graphs, repeated 100M-edge measurements on controlled hardware, update-density and degree-skew sweeps, allocator-normalized bytes-per-edge measurements, row-patch accumulation/consolidation experiments, and controlled compaction-policy ablations.

Artifacts are retained by the `Storage A/B Evidence` workflow as `velographx-storage-ab-10m` and `velographx-storage-ab-100m`.
