# NetworKit fair-benchmark contract (Issue #63)

This contract incorporates benchmarking guidance received from Mikhail Kirilin (KIT / NetworKit maintainer) on 11 September 2026. The guidance is **personal technical advice and is not a NetworKit endorsement of VeloGraphX, its benchmark results, or its claims**.

## What is implemented

The canonical NetworKit comparison is native C++. `benchmarks/external_networkit_fair_static.cpp` uses NetworKit's bulk `GraphBuilder`, then exercises the header-only callback traversals `NetworKit::Traversal::BFSfrom` and `NetworKit::Traversal::DijkstraFrom`. The callbacks record only the compact correctness information needed by the harness; no predecessor/path storage is requested. The existing native dynamic harness continues to exercise `NetworKit::DynBFS(graph, source, false)` and verifies every update batch against a fresh exact BFS.

The pinned NetworKit revision is 11.2.1 / `359f3fbf09b6d3fe214db24dd01bc8bfc1c2653c`. At that exact C++ revision the traversal APIs are callback-based; they do **not** have the Python-style `(false, false)` arguments sometimes used to describe disabling path storage. The callback forms used here are therefore the compile-verified C++ equivalent of the guidance, rather than a literal transcription of those arguments.

## Timing boundaries

Static/snapshot measurements report parsing, graph construction, and algorithm execution separately. Accepted result bundles must expose both algorithm-only latency and graph-build-plus-algorithm latency. Input parsing remains a separate field so graph construction is not confused with file-system performance.

Each measured algorithm receives one identical unmeasured warm-up. Repetitions use the already-built graph. BFS and unit-weight Dijkstra must agree on visited-vertex count and distance checksum on the contract run. Dynamic BFS keeps mutation plus answer restoration in its existing answer-ready envelope, while full-BFS verification remains outside the timed region.

## Build, threads, roots, and affinity

VeloGraphX and NetworKit comparison binaries use Release configuration and matched relevant flags (`-O3 -DNDEBUG -std=c++20 -fopenmp`). Workflows record compiler, CPU, software pins, OpenMP settings, and effective process affinity. Single-root contract tests run with `OMP_NUM_THREADS=1`, `OMP_PROC_BIND=true`, and `OMP_PLACES=cores`.

For future many-root parallel campaigns, roots are assigned to workers and each worker owns its own traversal state/result buffers; mutable algorithm objects must never be shared between worker threads. Where an object-based NetworKit BFS is used, one object is owned per worker and reused only where the NetworKit API safely supports rerunning it. The preferred `Traversal::BFSfrom` path is stateless at the API boundary, so each worker invokes it independently on the shared read-only graph.

## Dataset policy

`datasets/networkit-fair-benchmark-plan.json` is the machine-readable campaign contract. Pull-request CI validates the API/build/timing contract on bounded datasets. The expanded campaign intentionally mixes web, social, collaboration, road/infrastructure-like, and generated graph families. It names `com-LiveJournal`, `GAP-twitter`, `sk-2005`, `kron-27`, and `urand-27` as large/cache-scale targets, plus a generated grid family for infrastructure-like behavior.

The large datasets are **campaign targets, not claimed measurements in this PR**. They should run on hosted or controlled hardware only when acquisition, memory, runtime, and provenance constraints are satisfied. No performance number may be published merely because a dataset appears in the plan. Publication-grade conclusions still require controlled hardware and retained artifacts.

## Correctness and claim gates

A timing result is admissible only after exactness gates pass. Static BFS and unit-weight Dijkstra cross-check one another in the new harness. Dynamic `DynBFS` retains per-batch verification against a fresh exact BFS and cross-system final-layer checks where applicable. Any failing exactness gate invalidates the timing result rather than being relaxed.

Historical Python-binding NetworKit workflows remain useful provenance, but they are **legacy engineering evidence** and must not be used as the canonical language-neutral comparison after this contract. New stronger NetworKit claims must come from the native C++ contract.

## Issue #63 mapping

- Native C++: implemented.
- Header-only BFS/Dijkstra with no requested path storage: implemented with the pinned revision's callback APIs.
- `DynBFS(graph, source, false)`: already implemented in the native dynamic baseline and retained.
- Bulk graph construction: implemented with `GraphBuilder` in the static/snapshot harness.
- Object/thread ownership for many roots: contract defined; parallel campaign must use one worker-local state/object per thread.
- Affinity and environment recording: enforced by the workflow contract.
- Identical warm-up: one unmeasured warm-up per static algorithm; dynamic policy remains explicitly documented.
- Matched build mode/flags: enforced for the contract binaries.
- Algorithm-only and build-inclusive timing: implemented and emitted separately.
- Cache-scale graph: included as an explicit expanded-campaign requirement; results are not claimed until executed.
- Mixed graph domains and generated graphs: encoded in the machine-readable plan.
- Independent exactness: hard gate.
- Attribution: always described as Mikhail Kirilin's personal technical advice, never as NetworKit endorsement.
