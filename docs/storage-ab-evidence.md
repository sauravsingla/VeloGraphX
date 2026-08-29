# Dynamic-storage A/B evidence

This campaign compares VeloGraphX dynamic storage with the implementation immediately preceding the segmented-storage upgrade.

## Compared designs

**Historical baseline.** Reconstructed from commit `22d05c6b54b9199c852062395b3d6536abca02d9`: `std::vector<std::vector<VertexId>>` compact rows plus per-vertex `std::unordered_set<VertexId>` insertion and deletion deltas. It does not maintain reverse adjacency.

**Current design.** Fixed-size segmented CSR, shared packed sorted delta arenas, explicit transposed CSR and synchronized reverse deltas. Updates mark outgoing segments by source vertex and reverse segments by destination vertex. Explicit compaction rebuilds only dirty segments in each direction. Automatic maintenance uses per-segment delta density and arena fragmentation rather than forcing a global rebuild.

The historical implementation exists only inside the benchmark harness; production code is not switched between implementations.

## Workload

The hosted campaign creates deterministic directed degree-20 graphs. The 10M-edge case contains 500,000 vertices; the 100M-edge case contains 5,000,000 vertices. Each run applies a deterministic mixed batch equal to 0.1% of base edges, alternating deletions of known edges and insertions of known-absent edges.

Neighbor latency uses 8,192 deterministic probes after updates and before explicit compaction. A correctness gate requires identical sampled logical-neighborhood checksums and identical final directed edge counts. Linux RSS is sampled after releasing the generated input edge vector and requesting `malloc_trim(0)`. The 10M case uses three repetitions and medians; the 100M case is a single hosted execution.

## Baseline storage-upgrade result

The first complete A/B run (`33257865984`) measured the segmented/packed design before segment-local compaction was added.

| Metric | 10M current vs historical | 100M current vs historical |
|---|---:|---:|
| Loaded RSS | **0.967x** | **0.966x** |
| Mixed update throughput | **1.887x** | **1.332x** |
| Neighbor materialization latency | **0.784x** | **0.863x** |
| Bulk-load time | **2.855x** | **3.094x** |
| Full compaction time | **11.313x** | **16.159x** |
| Correctness | pass | pass |

The original absolute measurements were 110.5 versus 114.2 MiB RSS at 10M edges and 1,072.2 versus 1,109.7 MiB at 100M edges. The current design therefore used slightly less observed total RSS despite carrying a full reverse index, but its original global compaction path was substantially slower.

## Segment-local compaction follow-up

The production compaction path was then changed to track dirty forward and reverse segments independently, rebuild only marked segments, clear only the corresponding delta-row ranges, and use an automatic policy based on local delta density plus packed-arena fragmentation. Per-segment delta counts are maintained incrementally so the policy does not scan every vertex row when deciding whether to compact.

The identical A/B campaign was rerun at commit `984314b7647d442f2349d0d816c6331d7ef4e709` in workflow run `33259185211`.

| Metric | 10M | 100M |
|---|---:|---:|
| Loaded RSS, current / historical | **0.967x** | **0.966x** |
| Mixed update throughput, current / historical | **1.077x** | **1.278x** |
| Neighbor latency, current / historical | **0.793x** | **0.794x** |
| Compaction time, current / historical | **9.629x** | **11.989x** |
| Correctness | pass | pass |

Relative to the previous VeloGraphX compaction implementation, the historical-normalized compaction ratio fell from **11.313x to 9.629x at 10M edges** and from **16.159x to 11.989x at 100M edges**—about **14.9%** and **25.8%** lower respectively. This is a genuine improvement, but it does not eliminate the compaction disadvantage on this workload.

The same follow-up also shows an important trade-off: dirty-segment bookkeeping reduces the update-throughput advantage versus the earlier packed-delta implementation, especially at 10M edges. The repository therefore does not replace the stronger update-throughput numbers in the main README with this follow-up as if every metric improved simultaneously.

## Reverse-adjacency cost

For the regular degree-20 directed workload, reverse CSR contains the same number of arcs as forward CSR and therefore requires approximately the same compact base storage: about **42 MiB at 10M edges** and **420 MiB at 100M edges**. The first A/B campaign measured transpose construction at roughly 35 ms and 466 ms respectively.

This is an explicit functionality cost. It enables direct predecessor traversal for algorithms such as localized PageRank instead of global predecessor discovery.

## Interpretation

The evidence supports four narrow conclusions for the exercised workload:

1. Segmented CSR plus packed deltas retains lower observed RSS and lower neighbor-materialization latency than the historical per-row/per-hash-set layout.
2. The original packed-delta design produced the strongest update-throughput advantage in the baseline campaign.
3. Dirty-segment forward/reverse compaction materially reduces the previous global-compaction penalty, especially at 100M edges, but compaction remains slower than the historical row-local implementation when sparse updates are distributed broadly enough to dirty many large segments.
4. The remaining systems question is whether smaller/variable segments, row-level patching, or background amortized compaction can reduce that cost without giving back update throughput.

## Evidence boundary

These are hosted-CI engineering measurements on synthetic regular graphs, not universal or publication-grade claims. The 10M case is repeated; the 100M case is a single hosted execution. Runner CPU models can differ between workflow jobs, so comparisons are interpreted within each A/B job rather than as direct 10M-versus-100M scaling measurements.

Publication-quality evidence should add irregular real graphs, repeated 100M-edge measurements on controlled hardware, update-density and degree-skew sweeps, allocator-normalized bytes-per-edge measurements, and controlled compaction-policy ablations.

Artifacts are retained by the `Storage A/B Evidence` workflow as `velographx-storage-ab-10m` and `velographx-storage-ab-100m`.
