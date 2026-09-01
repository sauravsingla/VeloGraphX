# Storage-independent graph algorithm contract

VeloGraphX separates graph algorithms from graph representation so storage/layout effects can be measured independently from algorithmic effects.

## Design

The lightweight C++20 customisation layer is defined in `include/velographx/graph_access.hpp`. Generic algorithms use free-function accessors such as `vertex_count`, `is_directed`, `for_each_neighbor`, `for_each_in_neighbor`, `has_edge`, and `apply_updates` rather than depending on a concrete `DynamicGraph` type.

The accessors support two paths:

1. an in-tree graph can provide the corresponding member operation;
2. a foreign graph can opt in non-intrusively through ADL customisation hooks such as `vx_vertex_count`, `vx_is_directed`, `vx_for_each_neighbor`, `vx_for_each_in_neighbor`, `vx_has_edge`, and `vx_apply_updates`.

No inheritance, base class, or modification of the foreign graph type is required. This is a BGL-style non-intrusive customisation approach implemented with C++20 concepts rather than a claim of Boost.Graph API compatibility.

The default public names (`IncrementalBFS`, `IncrementalSSSP`, `IncrementalComponents`, `IncrementalKCore`, `IncrementalPageRank`, `IncrementalTriangleCount`) preserve the normal `DynamicGraph` API. Reusable implementations are `BasicIncremental*<Graph>` templates. Weighted SSSP currently keeps a specialised weighted-graph contract while sharing the common Dijkstra queue/relaxation engine.

## Backends

Two in-tree backends exercise the same algorithm implementations:

1. `DynamicGraph`: segmented CSR + packed deltas + sparse row patches + reverse adjacency.
2. `CsrGraph`: read-optimised CSR with reverse CSR for incoming traversal.

`CsrGraph` deliberately does not implement mutation. It is used as a read-optimised recomputation backend and as a controlled storage baseline.

A third, external backend is now exercised in CI through an ADL-only Teseo adapter. See [Teseo same-algorithm storage evidence](teseo-storage-evidence.md).

## Zero-allocation traversal

Hot algorithm paths use callback traversal (`for_each_neighbor` / `for_each_in_neighbor`) rather than materialising `std::vector` adjacency lists. `DynamicGraph::neighbors()` remains available as a compatibility/materialisation API, but the generic incremental algorithms do not need it. `WeightedDynamicGraph` likewise provides allocation-free callback traversal.

## Shared SSSP implementation

Weighted and unweighted SSSP share the queue/relaxation engine in `incremental/dijkstra.hpp`. The varying input is the edge-weight enumeration: unit cost for unweighted graphs and stored weights for weighted graphs.

## Controlled in-tree benchmark

`velographx_backend_bfs_benchmark` runs the exact same `BasicIncrementalBFS<Graph>::recompute()` implementation against `DynamicGraph` and `CsrGraph` and fails if distance vectors differ.

Example:

```bash
./build/velographx_backend_bfs_benchmark 100000 8 0
```

The JSON output reports both timings and `correctness_match:true`.

This benchmark answers a storage question: with algorithm semantics fixed, how much does the representation change recomputation latency?

## Correctness tests

`test_storage_independence.cpp` executes identical algorithm templates on `DynamicGraph` and `CsrGraph` for BFS, unweighted SSSP, connected components, k-core, triangle counting, and PageRank. Results are checked for exact equality where appropriate and a tight numerical tolerance for PageRank.

`test_graph_access_adl.cpp` goes further: it defines a foreign graph type with no VeloGraphX-style graph member API and adapts it only through ADL free functions. `BasicIncrementalBFS` and `BasicIncrementalSSSP` are then exercised through insertion and deletion updates. This is the regression gate for the non-intrusive customisation contract.

## External dynamic graph adapters

Teseo is now a real same-algorithm storage experiment rather than a future placeholder. The external workflow pins Teseo commit `2c37c2831c4d2acaaa838a86e1318363ce68c45b`, builds it from source, and runs the same `BasicIncrementalBFS::recompute()` implementation over `DynamicGraph`, `CsrGraph`, and the Teseo adapter. Full BFS distance vectors must match. The retained hosted evidence is documented in [teseo-storage-evidence.md](teseo-storage-evidence.md).

A fair external adapter must satisfy all of the following before benchmark numbers are reported:

- pin an immutable upstream revision/version;
- use identical graph inputs and update streams or snapshots as required by the experiment;
- preserve directed/undirected and duplicate/self-loop semantics;
- keep the VeloGraphX algorithm implementation fixed for a storage-swap experiment;
- separate conversion/build time from steady-state kernel time;
- validate outputs against an independent or cross-backend reference;
- report compiler flags, hardware, threads, NUMA placement and repetitions;
- distinguish storage-interface evidence from a system-level comparison of each project's native algorithms.

Sortledton remains a useful additional backend for independent replication. It is not folded into the current table until an equally pinned, same-semantics adapter is validated.

## Experiment families

### Storage swap

Hold algorithm and workload fixed:

```text
BasicIncrementalBFS::recompute
  -> DynamicGraph
  -> CsrGraph
  -> Teseo adapter
  -> another external adapter when semantics permit
```

### Incremental repair vs recomputation

Hold workload and correctness contract fixed, and report separately:

- incremental repair over mutable storage;
- full recomputation over the same mutable storage;
- full recomputation over read-optimised CSR.

This separation prevents a storage-layout advantage from being misreported as an incremental-algorithm advantage, and vice versa.
