\page dynamic_connected_components Dynamic Connected Components — Exact Connectivity on Evolving Graphs

Dynamic connected components keeps track of graph connectivity while edges are added or removed. Instead of recomputing all components after every update, a maintained algorithm can reuse existing connectivity state and update only what is necessary.

VeloGraphX provides exact maintained connectivity for evolving graphs through its dynamic connected-components implementation.

## What dynamic connected components solves

For an undirected graph, connected components partition vertices into groups where every pair of vertices in the same component is connected by a path. When the graph changes, insertions can merge components and removals can potentially split them.

A dynamic connected-components algorithm must update that partition correctly after each graph change. VeloGraphX treats the maintained result as an exact connectivity contract rather than an approximation.

## Why deletions are harder

An inserted edge can often be handled locally because it either connects vertices that are already in the same component or joins two components. A removed edge is more challenging because the engine must determine whether an alternative path still connects the affected vertices or whether a component has actually split.

This is a common theme in dynamic graph algorithms: the number of updated edges alone does not fully describe the amount of affected analytical work.

## VeloGraphX implementation context

The maintained connected-components implementation is part of the public incremental API under `include/velographx/incremental/connected_components.hpp`.

It runs over the same evolving-graph substrate used by the rest of VeloGraphX, so connectivity maintenance can be combined with repeated update batches, canonical graph consolidation, and other maintained analytics.

## Exact connectivity and correctness-first execution

VeloGraphX documents connected components as exact maintained connectivity. The project distinguishes exact-result algorithms from PageRank-related maintenance, which uses a residual/tolerance contract instead.

That explicit contract is useful for applications where changing graph structure must not silently change result semantics.

## Related VeloGraphX documentation

- [Dynamic graph algorithms](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Incremental graph analytics](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [Dynamic BFS](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [Dynamic triangle counting](https://sauravsingla.github.io/VeloGraphX/dynamic_triangle_counting.html)
- [C++ graph analytics library](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)
- [Project repository](https://github.com/sauravsingla/VeloGraphX)
