\page incremental_bfs Incremental BFS — Maintain Exact BFS After Graph Updates

Incremental BFS keeps breadth-first-search distances current as a graph changes. Instead of rerunning BFS from scratch after every edge update, an incremental algorithm can reuse the previous result and repair only state that may have changed.

VeloGraphX supports exact maintained BFS on evolving graphs. Localized repair and full recomputation are both available as exact execution choices, so the engine does not assume that incremental work is always the faster option.

## Incremental BFS versus static BFS

Static BFS computes unweighted shortest-path distances for one fixed graph snapshot. Incremental BFS solves the same problem repeatedly across graph snapshots connected by insertions and removals.

When affected state is small, localized repair can avoid scanning the full graph. When an update invalidates a large dependency region, full recomputation can be the better plan. VeloGraphX exposes that repair-versus-recompute crossover directly.

## Exactness first

The VeloGraphX BFS contract is exact unweighted shortest-path distance. Maintained results are checked against exact reference BFS in test and publication harnesses. A poor plan choice can increase latency, but it does not change the result contract.

The public C++ implementation is available under `include/velographx/incremental/bfs.hpp`, and the same native engine is also exposed through Python bindings.

Install the Python package with:

```bash
python -m pip install velographx
```

## Related VeloGraphX documentation

- [Dynamic BFS](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [Dynamic graph algorithms](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Incremental graph analytics](https://sauravsingla.github.io/VeloGraphX/incremental_graph_analytics.html)
- [C++ graph analytics library](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)
- [Project repository](https://github.com/sauravsingla/VeloGraphX)
