# VeloGraphX Architecture

## Design goal

VeloGraphX is a CPU-native graph analytics runtime for large evolving graphs. The architecture must allow fast static traversal today and hybrid dynamic/incremental execution in later milestones.

```text
Public C++ / Python API
        |
Graph + GraphVersion API
        |
Storage: CSR base + mutable delta (future)
        |
Algorithm runtime
   |            |
static      incremental
   |            |
frontier / affected-region engine
        |
adaptive graph kernels
        |
CPU runtime: scalar / SIMD / multicore / NUMA
        |
memory + I/O + metrics
```

## Milestone 1 implementation

The first implementation uses immutable CSR built from an edge list. Vertex IDs are dense `uint32_t` indices and edge offsets use `uint64_t`. Edges are sorted and deduplicated during construction. Undirected graphs materialize both directions.

The graph exposes constant-time degree lookup and contiguous neighbor spans. This keeps the baseline simple and provides a reference representation against which future dynamic storage is measured.

## Planned dynamic storage

A later `DynamicGraph` will maintain a mostly immutable CSR base plus mutable per-partition or per-vertex delta structures. Updates form atomic `UpdateBatch` objects that advance a monotonically increasing `GraphVersion`. Readers see stable versions. Compaction merges delta state into the base when a cost model predicts that traversal or memory penalties exceed rebuild cost.

## Algorithm runtime

Static algorithms consume a read-only graph view. Incremental algorithms will own persistent state and implement update hooks that receive a graph version transition and update batch. Shared runtime abstractions will include `AffectedSet`, sparse/dense `Frontier`, propagation queues, metrics, and an `ExecutionPlan` describing incremental or full recomputation.

## Adaptive planner

The planned planner compares estimated incremental work with estimated recomputation work. Inputs may include update fraction, affected-set estimate, frontier growth, degree statistics, algorithm class, recent timings, and hardware topology. The planner must expose its reasoning through an explain/metrics interface.

## Kernel layer

Neighborhood operations will use a portable scalar baseline and optional AVX2, AVX-512 and ARM NEON variants. Runtime dispatch selects the best available implementation. Intersection strategy may switch among linear merge, galloping, SIMD and bitmap methods according to calibrated crossover points.

## Parallel and NUMA runtime

The initial code is correctness-first and mostly single-threaded. Later milestones add configurable multicore scheduling, local queues/work stealing, affinity, first-touch allocation, NUMA-local partitions, and remote-memory measurements.

## Out-of-core direction

The storage and graph-view interfaces must not assume that all adjacency is permanently resident. A future partition manager can use mmap/io_uring, asynchronous prefetch, compressed cold partitions and a fixed memory budget without changing algorithm semantics.

## API stability

Public headers remain small. Experimental storage and runtime APIs should stay internal until semantics stabilize. Version 0.x may make breaking changes.