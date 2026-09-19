\page dynamic_triangle_counting Dynamic Triangle Counting — Exact Counts on Evolving Graphs

Dynamic triangle counting maintains the number of triangles in a graph as edges are inserted or removed. Triangles are a basic building block for clustering, motif analysis, network structure, and many graph-mining workloads, but recomputing a global triangle count after every graph update can be expensive.

VeloGraphX provides exact maintained triangle counting for evolving graphs and evaluates it against exact reference computation.

## What dynamic triangle counting does

A triangle is a set of three vertices connected pairwise by edges. When one edge changes, only triangles involving that edge can be created or destroyed, so a maintained algorithm can often update the exact global count by examining relevant local neighborhoods instead of recounting every triangle in the graph.

That local opportunity is the basis for dynamic triangle maintenance, but its cost still depends on graph structure and update regime.

## Exact count semantics

VeloGraphX documents triangle counting with an exact count contract. Maintained triangle results are checked against exact recomputation in tests and retained benchmark campaigns.

The public implementation is available under `include/velographx/incremental/triangles.hpp` and uses the same evolving-graph substrate as the other maintained analytics.

## Reproducible dynamic-triangle evidence

The repository retains exact dynamic-triangle experiments rather than reporting performance without a correctness check. One publication-facing comparison uses an exact external reference and keeps all paired exactness results alongside latency measurements. Additional multi-dataset crossover evidence is retained to show that graph structure can change the balance between localized maintenance and recomputation.

VeloGraphX does not generalize those scoped results into a claim that one triangle-counting strategy is universally faster.

## Why triangle counting is useful for dynamic graph analytics

Dynamic triangle counts can support repeatedly refreshed measures of local density, clustering, community structure, and network change. They are relevant when the underlying graph evolves continuously but exact motif counts still matter.

In VeloGraphX, triangle maintenance sits alongside dynamic BFS, connected components, k-core, weighted shortest paths, and PageRank-related workflows, allowing multiple graph analytics to share one mutable graph substrate.

## Related VeloGraphX documentation

- [Dynamic graph algorithms](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Incremental graph analytics](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [Dynamic connected components](https://sauravsingla.github.io/VeloGraphX/dynamic_connected_components.html)
- [Dynamic BFS](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [C++ graph analytics library](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)
- [Project repository](https://github.com/sauravsingla/VeloGraphX)
