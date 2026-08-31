# Storage-independent graph algorithm contract

VeloGraphX separates graph algorithms from graph representation so storage/layout effects can be measured independently from algorithmic effects.

## Design

The lightweight C++20 customisation layer is defined in `include/velographx/graph_access.hpp`. Algorithms depend on a small graph surface rather than a concrete `DynamicGraph` type:

- `vertex_count()`
- `directed()`
- zero-copy/callback outgoing traversal
- zero-copy/callback incoming traversal where required
- `has_edge()` where required
- `apply(batch)` for mutable incremental algorithms

No inheritance is required. A foreign representation can be used directly when it provides the required operations, or wrapped by a small adapter.

The default public names (`IncrementalBFS`, `IncrementalSSSP`, `IncrementalComponents`, `IncrementalKCore`, `IncrementalPageRank`, `IncrementalTriangleCount`) remain aliases for `DynamicGraph` specialisations, preserving source compatibility. The reusable implementations are `BasicIncremental*<Graph>` templates.

## Backends

Two in-tree backends exercise the same algorithm implementations:

1. `DynamicGraph`: segmented CSR + packed deltas + sparse row patches + reverse adjacency.
2. `CsrGraph`: read-optimised CSR with reverse CSR for incoming traversal.

`CsrGraph` deliberately does not implement mutation. It is used as a read-optimised recomputation backend and as a controlled storage baseline.

## Zero-allocation traversal

Hot algorithm paths use callback traversal (`for_each_neighbor` / `for_each_in_neighbor`) rather than materialising `std::vector` adjacency lists. `DynamicGraph::neighbors()` remains available as a compatibility/materialisation API, but the core incremental algorithms no longer rely on it. `WeightedDynamicGraph` likewise provides allocation-free callback traversal.

## Shared SSSP implementation

Weighted and unweighted SSSP share the queue/relaxation engine in `incremental/dijkstra.hpp`. The only varying input is the edge-weight enumeration: unit cost for unweighted graphs and stored weights for weighted graphs.

## Controlled benchmark

`velographx_backend_bfs_benchmark` runs the exact same `BasicIncrementalBFS<Graph>::recompute()` implementation against `DynamicGraph` and `CsrGraph` and fails if distance vectors differ.

Example:

```bash
./build/velographx_backend_bfs_benchmark 100000 8 0
```

The JSON output reports both timings and `correctness_match:true`.

This benchmark is intended to answer a storage question: with algorithm semantics fixed, how much does the representation change recomputation latency?

## Correctness test

`test_storage_independence.cpp` executes identical algorithm templates on `DynamicGraph` and `CsrGraph` for BFS, unweighted SSSP, connected components, k-core, triangle counting, and PageRank. Results are checked for exact equality where appropriate and a tight numerical tolerance for PageRank.

## External dynamic graph adapters

Teseo and Sortledton are appropriate candidates for future same-semantics storage experiments, but they are intentionally not mandatory build dependencies. A fair adapter must satisfy all of the following before benchmark numbers are reported:

- pin an immutable upstream revision/version;
- use identical graph inputs and update streams;
- preserve directed/undirected and duplicate/self-loop semantics;
- keep the VeloGraphX algorithm implementation fixed;
- separate conversion/build time from steady-state kernel time;
- validate outputs against an independent reference;
- report compiler flags, hardware, threads, NUMA placement and repetitions.

If an external system cannot expose the traversal/mutation semantics required by the same algorithm without changing the algorithm itself, it must be reported as a system-level comparison rather than a storage-swap experiment.

## Experiment families

### Storage swap

Hold algorithm and workload fixed:

```text
BasicIncrementalBFS::recompute
  -> DynamicGraph
  -> CsrGraph
  -> external adapter (when semantics permit)
```

### Incremental repair vs recomputation

Hold workload and correctness contract fixed, and report separately:

- incremental repair over mutable storage;
- full recomputation over the same mutable storage;
- full recomputation over read-optimised CSR.

This separation prevents a storage-layout advantage from being misreported as an incremental-algorithm advantage, and vice versa.
