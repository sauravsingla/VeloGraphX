\page dynamic_bfs Dynamic BFS — Exact Dynamic Breadth-First Search

Dynamic BFS maintains breadth-first-search results while a graph changes over time. Instead of rebuilding the entire BFS tree after every update, a dynamic graph engine can try to repair only the affected portion of the result. That approach can be much cheaper when updates are local, but full recomputation can still be preferable when an update touches a large or structurally important region.

VeloGraphX treats those two choices as explicit execution plans. For unweighted shortest paths, both localized repair and full recomputation preserve the same exact BFS distance contract. A pre-repair policy can choose between the plans using graph/update structure and measured execution history, while conservative fallback remains available when localized work expands beyond a useful region.

## What dynamic BFS means in VeloGraphX

For a fixed BFS source, graph updates can change reachability and shortest unweighted distances. VeloGraphX provides maintained BFS logic for evolving graphs and validates the maintained result against exact reference computation in its publication and engineering test harnesses.

The important systems distinction is between:

- **localized exact repair**, which revisits state affected by the update; and
- **exact full recomputation**, which rebuilds the BFS result from the current graph state.

VeloGraphX does not assume that incremental execution always wins. Its research artifact explicitly preserves workloads where recomputation or an external graph system is faster.

## Why repair versus recomputation matters

Dynamic BFS performance is highly workload-dependent. A small update fraction does not automatically imply small repair work: one edge deletion can invalidate a large dependency region, while a larger batch can sometimes remain structurally local. VeloGraphX therefore exposes repair/recompute selection as a physical-plan decision rather than hard-coding one strategy.

The publication-facing evidence includes exact cross-dataset BFS experiments, a production-style fallback replay, held-out workloads, feature ablations, and scoped comparisons with other graph systems. Negative selector results are retained so that the documented claim remains narrower than “dynamic BFS is always faster.”

## Exactness contract

For BFS and unweighted SSSP, VeloGraphX reports exact shortest-path distances. Plan-selection errors are therefore performance errors rather than correctness errors: choosing the slower exact plan can hurt latency, but it does not change the result contract.

The implementation is available in the public incremental BFS API under `include/velographx/incremental/bfs.hpp`, with additional reference and benchmark code in the repository.

## When to use dynamic BFS

Dynamic BFS is useful when a graph is repeatedly updated and the same source-to-vertex distance result must stay current. Typical examples include evolving networks, dependency graphs, communication graphs, fraud or transaction networks, infrastructure graphs, and interactive graph-analysis pipelines.

VeloGraphX is most relevant when you want both an exact result and an engine that can choose between maintenance and recomputation instead of committing to one execution mode for every update regime.

## Related VeloGraphX documentation

- [Incremental BFS](https://sauravsingla.github.io/VeloGraphX/incremental_bfs.html)
- [Dynamic graph algorithms](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Incremental graph analytics](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [C++ graph analytics library](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)
- [Project repository](https://github.com/sauravsingla/VeloGraphX)
