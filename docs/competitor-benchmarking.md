# Competitor benchmark protocol

VeloGraphX competitor measurements must be reproducible and machine-readable. The repository therefore provides `tools/competitor_benchmark.py` as a common adapter layer rather than maintaining unrelated one-off scripts for each library.

## Current adapters

The initial adapter set supports BFS for:

- `builtin`: a Python standard-library reference implementation used only to validate the harness and result schema.
- `networkx`: NetworkX graph construction and single-source shortest-path length.
- `igraph`: python-igraph graph construction and unweighted distance calculation.
- `networkit`: NetworKit graph construction and BFS.
- `rustworkx`: rustworkx graph construction and unit-weight Dijkstra as an unweighted-distance equivalent.

Competitor packages are optional dependencies. The benchmark runner fails explicitly if a requested package is unavailable rather than silently substituting another implementation.

## Example

```bash
python tools/competitor_benchmark.py \
  --dataset datasets/example-local-edge-list.txt \
  --framework networkx \
  --algorithm bfs \
  --source 0 \
  --repeat 5 \
  --output results/networkx-bfs.json
```

## Report schema

Each result uses schema version 1 and records at least:

- framework and framework version;
- algorithm and source vertex;
- dataset path and SHA-256;
- directedness, vertex count and edge count;
- all measured repetitions and the median elapsed time;
- deterministic digest of the algorithm result and reachable-vertex count;
- Python, OS, machine, processor and CPU-count metadata.

The result digest is intended for correctness comparison across implementations. Timing comparisons are valid only when all frameworks use the same dataset bytes, graph semantics, algorithm definition, warm-up policy, repetition count and machine configuration.

## Measurement discipline

The builtin adapter is not a performance competitor. It exists to make the harness testable without external dependencies. NetworkX, igraph, NetworKit and rustworkx results should be produced in a pinned benchmark environment and their package versions retained in every report.

SuiteSparse:GraphBLAS/LAGraph and GAP require native build/executable adapters rather than pretending their command-line interfaces are interchangeable with Python packages. Those adapters remain future work. Large-scale benchmark claims also remain unmeasured until the documented campaign is run on dedicated hardware.
