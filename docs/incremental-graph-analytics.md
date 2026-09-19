\page incremental_graph_analytics Incremental Graph Analytics for Evolving Graphs

Incremental graph analytics maintains analytical state as a graph changes instead of recomputing every result from scratch after each update. The approach is valuable for evolving graphs where updates arrive repeatedly and analytical freshness matters.

VeloGraphX combines mutable graph storage, maintained algorithms, exactness checks, and adaptive execution choices so incremental processing is treated as one physical plan rather than an unconditional rule.

## What incremental graph analytics means

An incremental graph engine tries to reuse prior analytical state after an update. The potential benefit is proportional to how much work can be avoided, not simply to how few edges changed. A small update can still affect a large dependency region, while a larger batch can remain structurally local.

VeloGraphX therefore separates correctness from execution strategy. Where an exact contract applies, localized maintenance and exact recomputation must produce the same analytical result even when their costs differ substantially.

## Mutable graph storage and maintained state

The VeloGraphX storage layer supports repeated updates through segmented CSR and mutable delta state, sparse row patches, forward/reverse adjacency, and explicit canonical consolidation. This lets analytical code operate on an evolving graph without forcing a complete canonical rebuild after every update batch.

Maintained analytics include BFS/unweighted SSSP, weighted SSSP, connected components, triangle counting, k-core, and PageRank-related workflows, each with an explicitly documented result contract.

## Repair versus recomputation

Incremental processing is not always faster. If repair discovers that affected work is too large, a system can end up paying for both repair discovery and subsequent recomputation. VeloGraphX's adaptive BFS work studies this crossover directly by keeping localized exact repair and exact full recomputation available as competing plans.

The project also retains negative held-out results and competitor wins. This keeps the claim focused on workload-aware execution rather than universal incremental superiority.

## Reproducible evaluation

VeloGraphX records dataset identity, competitor revisions, timing boundaries, exactness checks, retained repetitions, artifact hashes, and claim limitations. The paper artifact and results ledger map quantitative statements back to retained evidence.

That reproducibility discipline is especially important for incremental graph analytics because timing boundaries can otherwise hide update application, repair discovery, fallback, or recomputation costs.

## Related VeloGraphX documentation

- [Dynamic graph algorithms](https://sauravsingla.github.io/VeloGraphX/dynamic_graph_algorithms.html)
- [Dynamic BFS](https://sauravsingla.github.io/VeloGraphX/dynamic_bfs.html)
- [Incremental BFS](https://sauravsingla.github.io/VeloGraphX/incremental_bfs.html)
- [Dynamic connected components](https://sauravsingla.github.io/VeloGraphX/dynamic_connected_components.html)
- [Dynamic triangle counting](https://sauravsingla.github.io/VeloGraphX/dynamic_triangle_counting.html)
- [C++ graph analytics library](https://sauravsingla.github.io/VeloGraphX/cpp_graph_analytics_library.html)
