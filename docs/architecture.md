# VeloGraphX Architecture

## Design goal

VeloGraphX is a CPU-native graph analytics runtime for large evolving graphs. The current architecture supports both static traversal and dynamic/incremental execution while preserving exact results and exposing the crossover between localized repair and full recomputation.

```text
Public C++ / Python API
        |
Graph + GraphVersion / UpdateBatch APIs
        |
Storage: segmented CSR + packed deltas + sparse row patches
        |
forward + reverse adjacency
        |
Algorithm runtime
   |                    |
static              incremental
   |                    |
frontier / affected-region engine
        |
adaptive execution + graph kernels
        |
CPU runtime: scalar / SIMD / multicore / NUMA-aware policies
        |
memory + compression + partition I/O + metrics
```

## Static graph foundation

`CsrGraph` provides the compact static baseline. Vertex IDs use dense `uint32_t` indices and edge offsets use `uint64_t`. Edges are sorted and deduplicated during construction; undirected graphs materialize both directions.

The graph exposes constant-time degree lookup and contiguous neighbor spans. Static CSR remains both a fast traversal representation and the canonical reference used to validate dynamic and incremental paths.

## Dynamic storage

`DynamicGraph` stores the changing graph as a segmented CSR base plus packed mutable delta arenas and sparse row-level compact patches. Updates are grouped in `UpdateBatch` objects and advance graph state without requiring a full CSR rebuild on every change.

Forward and reverse adjacency are maintained together. This lets directed incremental algorithms traverse both outgoing and incoming neighborhoods without global predecessor scans.

Sparse compaction materializes only dirty logical rows. For long-running workloads, explicit canonical CSR consolidation can rebuild a clean segmented CSR snapshot when patch growth or measured access cost justifies it. Consolidation is intentionally separated from the normal update path so callers can validate a new snapshot before cutover.

See [Dynamic storage architecture](dynamic-storage.md) for the storage layout, maintenance controller, correctness contracts and retained measurements.

## Algorithm runtime

Static algorithms consume graph views through the graph-access abstraction. Incremental algorithms maintain persistent state and apply update batches to localized affected regions where possible.

The current codebase contains exact dynamic paths for BFS/unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core and PageRank-related maintenance. Correctness tests compare maintained state against recomputed references where required.

Shared runtime components cover sparse/dense frontiers, affected-region scheduling, execution planning, work stealing, NUMA-aware policies, partition management, metrics and storage/runtime introspection.

## Adaptive execution

VeloGraphX explicitly models the crossover between localized repair and full recomputation. Execution decisions can use update fraction, affected work, graph scale, root locality and observed cost.

The important contract is exactness: the planner chooses *how* to obtain the result, not whether correctness can be relaxed. When localized repair is no longer economical or a dependency is graph-wide, the runtime can fall back to full recomputation.

Planner and policy behavior are observable through retained benchmark evidence and execution-plan explanations rather than being treated as an opaque optimization.

## Kernel layer

Neighborhood operations have portable scalar implementations with optimized intersection and traversal paths where supported. The runtime includes CPU-feature-aware behavior, SIMD-oriented intersections and crossover-aware kernel selection.

Kernel choices are evaluated as engineering policies rather than assumed to dominate universally; retained benchmark evidence records both wins and negative results.

## Parallel and NUMA runtime

The repository includes multicore execution infrastructure, work-stealing and frontier schedulers, NUMA topology/policy abstractions, partitioning support and tests for these components. Hosted benchmark evidence includes multicore measurements, while controlled-hardware NUMA and larger scaling campaigns remain part of the publication-quality evaluation plan.

This distinction matters: the runtime capabilities are implemented, but shared GitHub runners are not treated as authoritative hardware for universal scaling claims.

## Memory, compression and partition I/O

The runtime includes compression policies, partition caching, partition-file support and asynchronous partition loading. Optional Linux `io_uring` integration can be enabled at build time when liburing is available.

These mechanisms are designed behind graph/storage abstractions so algorithm semantics do not depend on a single physical representation or residency policy.

## Graph-access abstraction

Algorithms are not restricted to one concrete storage class. The graph-access contract supports CSR, mutable VeloGraphX storage and foreign graph representations through compatible access adapters.

This separation allows storage experiments to reuse the same algorithm implementation and makes storage-interface comparisons distinct from comparisons against another system's own graph algorithms.

## Correctness and evidence boundary

VeloGraphX treats exactness, reproducibility and benchmark provenance as architecture concerns. Tests cover dynamic mutation, deletions, storage independence, graph-access adaptation, compression, temporal behavior, randomized updates, scheduling and partition I/O. Benchmark tooling retains dataset provenance, competitor revisions, result artifacts and explicit claim boundaries.

Hosted-CI measurements are engineering evidence, not publication-grade hardware claims. Controlled-hardware runs, broader graph families, larger repeated workloads, hardware counters and genuine NUMA placement remain necessary for publication-quality performance conclusions.

## API stability

Public headers remain intentionally focused, while storage and runtime components continue to evolve. Version 0.x may make breaking changes; users should pin a version or commit for reproducible research and benchmark comparisons.
