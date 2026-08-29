# Dynamic-storage A/B evidence

This campaign compares the current VeloGraphX dynamic storage layer with the historical implementation that immediately preceded the segmented-storage upgrade.

## Compared designs

**Historical baseline.** Reconstructed from commit `22d05c6b54b9199c852062395b3d6536abca02d9`: `std::vector<std::vector<VertexId>>` compact rows plus one `std::unordered_set<VertexId>` insertion delta and one deletion delta per vertex. It does not maintain reverse adjacency.

**Current design.** Fixed-size segmented CSR, shared packed sorted delta arenas, and an explicit transposed CSR plus synchronized reverse deltas for directed graphs.

The baseline is benchmark-only code. Production code is not switched between implementations.

## Workload

The hosted campaign constructs deterministic directed synthetic graphs with degree 20. The 10M-edge case contains 500,000 vertices; the 100M-edge case contains 5,000,000 vertices. Each campaign applies a deterministic mixed batch equal to 0.1% of the base edge count: alternating deletions of known base edges and insertions of known absent edges.

Neighbor-access latency uses 8,192 deterministic vertex probes after the update batch and before compaction. A correctness gate requires identical sampled logical-neighborhood checksums and identical final directed edge counts between the two designs.

Linux RSS is sampled from `/proc/self/status` after the generated input edge vector is released and `malloc_trim(0)` is requested. This reduces, but does not eliminate, allocator and hosted-runner noise. The 10M case is repeated three times and reports medians. The 100M case is a single hosted execution and should therefore be treated as scale evidence rather than a low-noise publication result.

## Results

| Metric | 10M historical | 10M current | 100M historical | 100M current |
|---|---:|---:|---:|---:|
| Directed edges | 10,000,000 | 10,000,000 | 100,000,000 | 100,000,000 |
| Vertices | 500,000 | 500,000 | 5,000,000 | 5,000,000 |
| Resident memory after load | 114.2 MiB | 110.5 MiB | 1,109.7 MiB | 1,072.2 MiB |
| Bulk-load time | 114.39 ms | 326.60 ms | 1,096.34 ms | 3,391.85 ms |
| Mixed update throughput | 2.895M/s | 5.462M/s | 2.511M/s | 3.346M/s |
| Neighbor materialization | 315.7 ns/probe | 247.4 ns/probe | 362.3 ns/probe | 312.7 ns/probe |
| Full compaction | 5.92 ms | 66.97 ms | 56.40 ms | 911.37 ms |
| Correctness gate | pass | pass | pass | pass |

Derived ratios, current relative to historical:

| Metric | 10M | 100M | Interpretation |
|---|---:|---:|---|
| Loaded RSS | **0.967x** | **0.966x** | about 3.3–3.4% lower total RSS |
| Update throughput | **1.887x** | **1.332x** | higher is better |
| Neighbor latency | **0.784x** | **0.863x** | about 21.6% / 13.7% lower latency |
| Bulk-load time | **2.855x** | **3.094x** | current load is slower |
| Full compaction time | **11.313x** | **16.159x** | current global rebuild is substantially slower |

## Reverse-adjacency cost

The current directed design intentionally pays for predecessor access that the historical layout did not provide.

| Metric | 10M edges | 100M edges |
|---|---:|---:|
| Forward segmented CSR estimate | 42.0 MiB | 419.6 MiB |
| Reverse segmented CSR estimate | 42.0 MiB | 419.6 MiB |
| Reverse / forward base storage | 100% | 100% |
| RSS increase after adding transpose | 42.1 MiB | 420.0 MiB |
| Transpose construction time | 35.43 ms | 466.31 ms |

For this regular degree-20 graph, the transpose has the same number of arcs and therefore essentially the same compact CSR storage as the forward direction. This is an explicit functionality cost, not hidden overhead. The benefit is O(indegree)-style predecessor access for algorithms such as localized PageRank instead of global predecessor discovery.

## What the experiment establishes

The packed/segmented architecture improves the two dynamic-path metrics it was primarily intended to improve in this workload: mixed update throughput and logical neighbor access. It also slightly reduces observed total RSS even though the current directed representation carries a full reverse index that the historical design lacks.

The experiment also exposes a real remaining weakness: `DynamicGraph::compact()` is a global rebuild of the outgoing CSR followed by reconstruction of the transpose. The historical implementation compacted only rows with deltas. Consequently, the new design's full-compaction cost is much higher in this sparse-update experiment. This result argues for incremental/segment-local compaction or a background amortized compaction policy rather than a claim that the current compaction path is already optimal.

Bulk loading is also slower because the current path globally sorts/deduplicates arcs and builds the transpose. Load-time optimization is secondary to dynamic-update behavior, but the cost should be retained in any paper table rather than omitted.

## Evidence boundary

These are reproducible hosted-CI engineering measurements on synthetic regular graphs, not universal claims about graph structure, allocator behavior, or production hardware. Publication-quality evidence should add irregular real graphs, repeated 100M-edge measurements on controlled hardware, bytes-per-edge/RSS counters under the same allocator, update-density sweeps, degree-skew sweeps, and segment-local compaction experiments.

Workflow: `Storage A/B Evidence`. The first complete 10M/100M run is GitHub Actions run `33257865984` at commit `f2f221945d8d423e1c81b676e52367049d6e93b9`. Artifacts are retained as `velographx-storage-ab-10m` and `velographx-storage-ab-100m`.
