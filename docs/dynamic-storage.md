# Dynamic storage architecture

VeloGraphX stores a changing graph as an immutable, cache-friendly base plus a mutable overlay. The design is intended to avoid the per-vertex and per-edge allocation overhead of a `vector<vector<VertexId>>` base combined with `unordered_set` delta tables.

## Segmented CSR base

The compact base is split into fixed-size vertex segments (65,536 vertices per segment). Each segment contains:

- one contiguous CSR offset array;
- one contiguous sorted edge array;
- zero-copy neighbor spans for compact algorithms.

Segmenting the vertex space keeps CSR rows contiguous while allowing the graph to extend its vertex range without rebuilding all existing base segments merely to add empty vertices. Bulk loading sorts and deduplicates arcs once and constructs the segmented CSR directly.

## Packed mutable deltas

Updates are represented in a shared packed arena. Each vertex owns metadata describing a contiguous sorted slice of delta entries in that arena. An entry records a destination and the desired presence state relative to the compact base.

This removes one hash table per vertex and avoids hash-node allocation per changed edge. Lookup within a delta row uses binary search. When a row outgrows its slice it is relocated with geometric capacity growth; old arena space is reclaimed by lightweight delta repacking or by full graph compaction.

An update that returns an edge to its base state removes the overlay entry instead of retaining a redundant insertion or tombstone. The delta ratio therefore measures live divergence from the compact base rather than update history.

## Reverse adjacency

`DynamicGraph` maintains a transposed segmented CSR and a matching packed reverse delta overlay. Directed algorithms can call `in_neighbors(v)` without scanning every source vertex. The reverse view is updated in the same operation as the forward view, and graph compaction rebuilds the transpose from the newly compacted forward base.

This is particularly important for localized PageRank repair: predecessor traversal becomes proportional to the actual incoming neighborhood instead of requiring a global vertex scan for every active destination.

## Compaction

`compact()` materializes each logical outgoing row once, rebuilds segmented CSR from those sorted rows, derives reverse CSR from the rebuilt base, and clears both packed delta arenas. Logical graph versioning remains tied to graph updates rather than representation-only compaction.

`maybe_compact(threshold)` retains the existing adaptive interface. Delta arenas can also repack themselves when relocation fragmentation becomes high, avoiding an unnecessary full graph rebuild.

## Introspection

The dynamic graph exposes:

- `base_edge_count_directed()` — compact forward-base arcs;
- `delta_edge_count()` — live forward overlay entries;
- `delta_ratio()` — live overlay / compact-base ratio;
- `storage_bytes()` — approximate owned storage for forward/reverse base and delta structures;
- `compact_neighbors()` and `compact_in_neighbors()` — zero-copy spans when callers know the graph is compact;
- `neighbors()` and `in_neighbors()` — logical adjacency with overlays applied.

## Correctness contract

Forward and reverse logical views must agree for every directed arc. Bulk-load duplicates are removed. Overlay entries are kept sorted and unique per vertex. Compaction must preserve logical outgoing and incoming rows exactly. Tests exercise insertion, deletion, cancellation back to base state, reverse traversal, compaction equivalence, edge counts, and vertex-space growth across storage segments.

## Current boundary

The new layout substantially reduces allocation and hash overhead and removes the previous global predecessor scan from localized PageRank. It is still an in-memory research engine: publication-grade claims about billion-edge memory efficiency should be based on measured resident memory and update throughput on controlled hardware rather than inferred from the data structure alone.
