# Storage-independent graph algorithm contract

VeloGraphX separates graph algorithms from graph representation so storage/layout effects can be measured independently from algorithmic effects.

## Design

The lightweight C++20 customisation layer is defined in `include/velographx/graph_access.hpp`. Generic algorithms use free-function accessors such as `vertex_count`, `is_directed`, `for_each_neighbor`, `for_each_in_neighbor`, `has_edge`, and `apply_updates` rather than depending on a concrete `DynamicGraph` type.

Weighted algorithms use the same non-intrusive model through `for_each_weighted_neighbor` and `edge_weight`. A foreign weighted graph can provide `vx_for_each_weighted_neighbor` and `vx_edge_weight` without inheriting from, wrapping, or modifying a VeloGraphX storage class.

The accessors support two paths:

1. an in-tree graph can provide the corresponding member operation;
2. a foreign graph can opt in non-intrusively through ADL customisation hooks such as `vx_vertex_count`, `vx_is_directed`, `vx_for_each_neighbor`, `vx_for_each_in_neighbor`, `vx_has_edge`, `vx_apply_updates`, `vx_for_each_weighted_neighbor`, and `vx_edge_weight`.

No inheritance, base class, or modification of the foreign graph type is required. This is a BGL-style non-intrusive customisation approach implemented with C++20 concepts rather than a claim of Boost.Graph API compatibility.

The default public names (`IncrementalBFS`, `IncrementalSSSP`, `IncrementalComponents`, `IncrementalKCore`, `IncrementalPageRank`, `IncrementalTriangleCount`, `IncrementalWeightedSSSP`) preserve the normal in-tree API. Reusable implementations are `BasicIncremental*<Graph>` templates, including `BasicIncrementalWeightedSSSP<Graph>`.

## Backends

Two in-tree backends exercise the same unweighted algorithm implementations:

1. `DynamicGraph`: segmented CSR + packed deltas + sparse row patches + reverse adjacency.
2. `CsrGraph`: read-optimised CSR with reverse CSR for incoming traversal.

`CsrGraph` deliberately does not implement mutation. It is used as a read-optimised recomputation backend and as a controlled storage baseline.

Three independent external representations now exercise the same non-intrusive contract in CI:

1. **Teseo**, pinned and built from source, supports both fixed-algorithm recomputation and mixed insertion/deletion batches through `BasicIncrementalBFS::apply()`.
2. **Boost.Graph `adjacency_list`**, adapted only through ADL hooks, provides a second independently implemented mutable storage representation and is checked through the same recomputation and incremental-update sequence.
3. **Sortledton**, pinned at commit `6eb638f3ad38f8a10a127e7e118528f4c8d07a6e`, is built from source and exercised through the same fixed `BasicIncrementalBFS` algorithm for recomputation and deterministic mixed insertion/deletion batches.

The Sortledton workflow records its immutable upstream sources and applies one documented constructor-validation compatibility guard for GCC 10. The guard replaces a power-of-two check with an equivalent nonzero bit test; it does not change graph storage, update, or transaction-manager semantics. All three implementations are built with GCC/G++ 10, and complete BFS distance vectors must match after recomputation and every update batch. This is storage-portability and correctness evidence, not a publication-grade system performance comparison.

## Zero-allocation traversal

Hot algorithm paths use callback traversal (`for_each_neighbor` / `for_each_in_neighbor`) rather than materialising `std::vector` adjacency lists. `DynamicGraph::neighbors()` remains available as a compatibility/materialisation API, but the generic incremental algorithms do not need it. `WeightedDynamicGraph` likewise provides allocation-free callback traversal.

## Shared and storage-independent SSSP

Weighted and unweighted SSSP share the queue/relaxation engine in `incremental/dijkstra.hpp`. Unweighted SSSP supplies unit cost. `BasicIncrementalWeightedSSSP<Graph>` obtains stored weights through the weighted graph-access hooks and applies update batches through the same non-intrusive mutation path.

`tests/test_weighted_sssp.cpp` defines a foreign weighted graph that exposes no VeloGraphX storage API. It is adapted only with `vx_*` functions and must match `WeightedDynamicGraph` exactly after initial computation, a weight decrease, insertion, weight increase, and deletion. This is the regression gate for weighted storage independence.

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

`test_graph_access_adl.cpp` goes further: it defines a foreign graph type with no VeloGraphX-style graph member API and adapts it only through ADL free functions. `BasicIncrementalBFS` and `BasicIncrementalSSSP` are then exercised through insertion and deletion updates. This is the regression gate for the non-intrusive unweighted customisation contract.

`test_weighted_sssp.cpp` provides the analogous weighted ADL regression described above.

## External dynamic graph adapters

Teseo is a real same-algorithm mutable-storage experiment. The external workflow pins Teseo commit `2c37c2831c4d2acaaa838a86e1318363ce68c45b`, builds it from source, and runs the same `BasicIncrementalBFS` implementation over `DynamicGraph`, `CsrGraph`, and the Teseo adapter. It checks complete BFS distance-vector equality for recomputation and then checks `DynamicGraph` versus Teseo after every mixed incremental batch. See [Teseo same-algorithm storage evidence](teseo-storage-evidence.md).

The Boost.Graph workflow independently compiles an `adjacency_list<vecS, vecS, undirectedS>` adapter and performs the same full-vector recomputation and incremental correctness gates. It is evidence that the interface is not accidentally specialised to VeloGraphX or Teseo.

The Sortledton workflow independently builds the pinned upstream storage library, compiles `benchmarks/external_sortledton_storage_bfs.cpp`, and runs five repetitions at 2,048 and 8,192 vertices. It requires exact recomputation equality and exact incremental equality after all five deterministic mixed-update batches at both sizes. Timings are retained only as hosted-CI diagnostics and are explicitly gated from publication claims.

A fair external adapter must satisfy all of the following before benchmark numbers are reported:

- pin an immutable upstream revision/version when an external source tree is required;
- use identical graph inputs and update streams or snapshots as required by the experiment;
- preserve directed/undirected and duplicate/self-loop semantics;
- keep the VeloGraphX algorithm implementation fixed for a storage-swap experiment;
- separate conversion/build time from steady-state kernel time;
- validate outputs against an independent or cross-backend reference;
- report compiler flags, hardware, threads, NUMA placement and repetitions when making performance claims;
- distinguish storage-interface evidence from a system-level comparison of each project's native algorithms.

## Experiment families

### Storage swap

Hold algorithm and workload fixed:

```text
BasicIncrementalBFS
  -> DynamicGraph
  -> CsrGraph (recompute only)
  -> Teseo adapter
  -> Boost.Graph adapter
  -> Sortledton adapter
```

### Incremental repair vs recomputation

Hold workload and correctness contract fixed, and report separately:

- incremental repair over mutable storage;
- full recomputation over the same mutable storage;
- full recomputation over read-optimised CSR.

This separation prevents a storage-layout advantage from being misreported as an incremental-algorithm advantage, and vice versa.
