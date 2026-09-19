\page dynamic_graph_algorithms Dynamic Graph Algorithms in C++20 and Python

Dynamic graph algorithms update analytical results as the underlying graph evolves. They are useful when a graph receives a stream or sequence of edge updates and repeatedly recomputing every analytical result from scratch would be unnecessarily expensive.

VeloGraphX is a C++20 and Python engine for dynamic graph analytics. It provides maintained or dynamic execution paths for BFS and unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank-related workflows.

## Supported dynamic graph analytics

VeloGraphX exposes dynamic or maintained support for:

- **BFS / unweighted SSSP** with exact distance semantics;
- **weighted SSSP** with exact distances and conservative recomputation fallback;
- **connected components** with exact maintained connectivity;
- **triangle counting** with exact maintained counts;
- **k-core** with exact core-number maintenance; and
- **PageRank** with residual/tolerance validation and conservative fallback rather than a mathematical exactness claim.

The distinction in the final item is intentional: VeloGraphX documents algorithm-specific correctness contracts instead of applying one blanket exactness statement to every analytic.

## Dynamic execution on an evolving graph

The engine combines mutable graph storage with maintained analytics. Updates can be represented in mutable state without rebuilding the canonical graph representation after every batch. Analytical execution can then use localized maintenance where appropriate.

For the paper's strongest adaptive-plan result, VeloGraphX keeps both exact localized repair and exact full recomputation available for BFS and allows a pre-repair policy to choose between them. This makes the execution strategy observable and measurable rather than an implicit implementation detail.

## Why dynamic graph algorithms need workload-aware execution

The cost of a dynamic update depends on more than the number of changed edges. Graph topology, source position, update type, dependency structure, and the size of affected state can all change whether local repair is cheaper than recomputation.

VeloGraphX therefore retains both positive and negative benchmark results. Its documentation does not claim that dynamic execution always wins or that one strategy is universally optimal.

## C++20 and Python

The core engine is implemented in C++20 and exposes a native API. Python bindings make the same engine available for Python workflows and interoperability with common scientific-computing data structures.

This makes VeloGraphX useful both as a research systems artifact and as a graph analytics library for developers evaluating dynamic algorithms on evolving data.

## Related VeloGraphX documentation

- [Dynamic BFS](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [Incremental BFS](https://sauravsingla.github.io/VeloGraphX/incremental_bfs.html)
- [Incremental graph analytics](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [Dynamic connected components](https://sauravsingla.github.io/VeloGraphX/dynamic_connected_components.html)
- [Dynamic triangle counting](https://sauravsingla.github.io/VeloGraphX/dynamic_triangle_counting.html)
- [C++ graph analytics library](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)
