# Competitor benchmark protocol

VeloGraphX competitor measurements must be reproducible and machine-readable. The repository therefore provides `tools/competitor_benchmark.py` as a common adapter layer rather than maintaining unrelated one-off scripts for each library.

## Current adapters

The adapter set supports BFS for:

- `builtin`: a Python standard-library reference implementation used only to validate the harness and result schema.
- `networkx`: NetworkX graph construction and single-source shortest-path length.
- `igraph`: python-igraph graph construction and unweighted distance calculation.
- `networkit`: NetworKit graph construction and BFS.
- `rustworkx`: rustworkx graph construction and unit-weight Dijkstra as an unweighted-distance equivalent.
- `external`: a native-command contract intended for engines such as GAP and SuiteSparse:GraphBLAS/LAGraph wrappers without pretending those projects share one command-line API.

Competitor packages are optional dependencies. The benchmark runner fails explicitly if a requested package is unavailable rather than silently substituting another implementation.

## Python-library example

```bash
python tools/competitor_benchmark.py \
  --dataset datasets/example-local-edge-list.txt \
  --framework networkx \
  --algorithm bfs \
  --source 0 \
  --repeat 5 \
  --output results/networkx-bfs.json
```

## External native-command contract

An external command receives the benchmark request through environment variables:

- `VELOGRAPHX_DATASET`: absolute path to the edge-list dataset;
- `VELOGRAPHX_ALGORITHM`: currently `bfs`;
- `VELOGRAPHX_SOURCE`: source vertex;
- `VELOGRAPHX_DIRECTED`: `1` or `0`;
- `VELOGRAPHX_VERTICES`: inferred vertex count.

The command must exit successfully and write exactly one JSON object to stdout containing `distances`, a list with one integer distance per vertex where `-1` means unreachable. It may also provide `framework_version`. VeloGraphX measures the complete external-command elapsed time, validates the result shape, computes the same deterministic result digest used by in-process adapters, and records the requested external engine name.

Example:

```bash
python tools/competitor_benchmark.py \
  --dataset datasets/example-local-edge-list.txt \
  --framework external \
  --external-command './build/gap_bfs_adapter' \
  --external-name GAP \
  --repeat 5 \
  --output results/gap-bfs.json
```

`tools/mock_external_bfs.py` is a CI-only contract fixture. It is not a performance competitor and must not be cited as one.

## Report schema

Each result uses schema version 1 and records at least:

- framework, adapter type and framework version;
- algorithm and source vertex;
- dataset path and SHA-256;
- directedness, vertex count and edge count;
- all measured repetitions and the median elapsed time;
- deterministic digest of the algorithm result and reachable-vertex count;
- Python, OS, machine, processor and CPU-count metadata.

The result digest is intended for correctness comparison across implementations. Timing comparisons are valid only when all frameworks use the same dataset bytes, graph semantics, algorithm definition, warm-up policy, repetition count and machine configuration.

## Measurement discipline

The builtin adapter and mock external adapter are test references, not performance competitors. NetworkX, igraph, NetworKit and rustworkx results should be produced in a pinned benchmark environment and their package versions retained in every report. The generic external contract remains available for locally installed systems. In addition, `.github/workflows/hosted-native-competitors.yml` now builds pinned SuiteSparse:GraphBLAS/LAGraph and GAP sources and executes correctness-gated BFS/SSSP measurements; `.github/workflows/external-networkit-native-multidataset.yml` performs the matched native dynamic-BFS campaign. These hosted results are engineering evidence, not substitutes for the dedicated canonical-dataset campaign.

Large-scale benchmark claims remain unmeasured until the documented campaign is run on dedicated hardware.
