# Dynamic storage architecture

VeloGraphX stores a changing graph as a cache-friendly compact base plus mutable overlays. The current design combines large segmented CSR for locality, packed deltas for fast sparse updates, sparse row-level compact patches so explicit compaction does not rebuild whole 65K-row segments, and an explicit canonical-CSR consolidation path for long-running patch accumulation.

## Segmented CSR base

The primary compact base is split into fixed-size vertex segments of 65,536 vertices. Each segment contains one contiguous CSR offset array and one contiguous sorted edge array. Untouched rows therefore retain the locality and zero-copy access of the original segmented CSR design.

Bulk loading sorts and deduplicates arcs and constructs both the forward CSR and its transpose.

## Packed mutable deltas

Updates are represented in shared packed arenas. Each vertex owns metadata for a contiguous sorted slice of delta entries. An entry records a destination and the desired presence state relative to the current compact row.

This avoids one hash table per vertex and hash-node allocation per changed edge. Lookup within a delta row uses binary search. An update that restores an edge to its compact state removes the overlay entry, so `delta_ratio()` measures live divergence rather than update history.

## Row-level compact patches

A compacted sparse row no longer requires rebuilding its full 65,536-vertex CSR segment. Instead, VeloGraphX materializes only the touched logical row and stores it in a sparse compact-row patch table. Subsequent reads use the patch for that row and the original CSR for untouched rows.

Forward and reverse rows are patched independently. This preserves the large-CSR layout for the overwhelming majority of rows while making sparse explicit compaction proportional to the rows actually touched by updates.

Patched rows remain mutable: later updates are represented as packed deltas relative to the patch, and later compaction replaces that row's patch with a newly materialized compact row.

## Reverse adjacency

`DynamicGraph` maintains a transposed segmented CSR, reverse packed deltas, and reverse compact-row patches. Directed algorithms can call `in_neighbors(v)` without scanning every source vertex. Forward and reverse views are updated together.

This is particularly important for localized PageRank repair because predecessor traversal becomes proportional to the actual incoming neighborhood rather than requiring global predecessor discovery.

## Adaptive maintenance

`compact()` sorts and deduplicates the dirty-row identifiers, materializes only those forward and reverse rows, clears only their delta entries, and repacks the delta arenas.

`maybe_compact(threshold)` can compact individual rows whose delta density crosses a row-local threshold. Automatic row compaction is additionally gated by a global delta ratio: while global divergence is below 1%, sparse batches remain on the packed-delta path rather than paying a dirty-row sort/scan after every batch. Delta arenas can repack independently when relocation fragmentation becomes high.

This policy separates cheap mutation for sparse changes from localized row maintenance. It deliberately does not perform a global CSR rebuild inside the update path.

## Canonical CSR consolidation

Long-running row-local compaction can accumulate enough patched rows to increase owned storage and add a second compact-row lookup on a growing fraction of accesses. `include/velographx/storage/consolidation.hpp` provides `consolidate_to_csr_snapshot()`, which materializes the current logical graph into a fresh segmented CSR plus transpose.

The source graph is left unchanged. The returned snapshot starts with no row patches and no pending deltas, so an application can validate the snapshot before an explicit maintenance-boundary cutover. Consolidation is O(E) and is intentionally separated from `DynamicGraph::apply()`.

The same header provides `ConsolidationPolicy` and `evaluate_consolidation()`. The current engineering defaults signal consolidation when either owned storage or sampled neighbor latency reaches **1.25x** its canonical-CSR baseline. The helper only returns a signal; it never starts consolidation automatically.

Those defaults come from the retained real-graph accumulation campaign, not from an assumption that one threshold is universally optimal. Applications can provide their own policy values.

## Introspection

The dynamic graph exposes:

- `base_edge_count_directed()` — arcs represented by the logical compact layer, including row patches;
- `delta_edge_count()` — live forward overlay entries;
- `delta_ratio()` — live overlay / compact-edge ratio;
- `storage_bytes()` — approximate owned storage for forward/reverse CSR, patches and deltas;
- `compact_neighbors()` and `compact_in_neighbors()` — zero-copy spans from CSR or compact row patches;
- `neighbors()` and `in_neighbors()` — logical adjacency with overlays applied.

The existing dirty-count introspection names are retained for compatibility, but under the row-local design they count distinct dirty rows rather than physical CSR segments.

## Correctness contract

Forward and reverse logical views must agree for every directed arc. Bulk-load duplicates are removed. Overlay entries are sorted and unique per row. Row compaction must preserve outgoing and incoming neighborhoods, edge counts and later mutability.

CSR consolidation has an additional contract: the fresh snapshot must preserve the full logical adjacency, deterministic sampled-neighborhood digest, directed edge count and directed/undirected semantics before a caller cuts over. Tests cover snapshot preservation and consolidation-policy threshold behavior in addition to mutation, reverse traversal and row-patch maintenance.

## Measured boundary

The retained 10M/100M storage A/B campaign shows why row patches replaced whole-segment compaction: on the exercised 0.1% mixed-update workload, explicit compaction is about **1.657x** and **1.492x** the historical row-local implementation, versus **9.629x** and **11.989x** for the previous 65K dirty-segment design. The same final run retains lower neighbor latency and higher update throughput than the historical layout at both tested scales.

The follow-up real-graph campaign measures long-running accumulation on `ca-GrQc` and directed `web-Google`. On web-Google, 50 cycles raise sampled neighbor latency to **1.719x** the pristine CSR baseline while owned storage reaches **1.332x**; validated CSR consolidation reduces the sampled latency to **0.604x of the pre-consolidation value** and the owned footprint to **0.751x of the accumulated value**. See [row-patch accumulation evidence](row-patch-accumulation-evidence.md).

These are hosted-CI engineering measurements, not universal claims. Publication-quality follow-up still requires controlled hardware, broader graph families, mutation-locality sweeps, repeated steady-state consolidation cycles and maintenance-amortized application throughput.
