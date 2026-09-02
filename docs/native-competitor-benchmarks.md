# Native competitor benchmark recipes

VeloGraphX normalizes native competitors through `tools/competitor_benchmark.py --framework external`. The repository includes shims for SuiteSparse:GraphBLAS/LAGraph and the GAP Benchmark Suite and does not vendor those projects. For hosted engineering evidence, `.github/workflows/hosted-native-competitors.yml` clones immutable revisions, builds them from source and records versions, compiler flags and threading configuration with the artifact. Dedicated environments must still provide and record their own installations through the normalized contract.

## Common runner contract

A native BFS runner used by either shim must accept:

```text
--dataset PATH --source N --vertices N [--directed]
```

and write one JSON object to stdout containing at least:

```json
{"distances":[0,1,2,-1],"framework_version":"native-version"}
```

The distance array must contain exactly one integer per vertex, with `-1` for unreachable vertices. Any diagnostic text belongs on stderr, not stdout.

## SuiteSparse:GraphBLAS / LAGraph

Build SuiteSparse:GraphBLAS and LAGraph using their upstream instructions on the target benchmark machine. Build a small BFS driver against that installation which implements the common runner contract above. Then point VeloGraphX at the resulting executable:

```bash
export VELOGRAPHX_LAGRAPH_BFS_BIN=/absolute/path/to/lagraph_bfs_runner
python tools/competitor_benchmark.py \
  --dataset /absolute/path/to/graph.edgelist \
  --framework external \
  --external-command "python tools/native_competitors/lagraph_wrapper.py" \
  --external-name lagraph \
  --repeat 5 \
  --output results/lagraph-bfs.json
```

The shim validates the dataset and runner, forwards source/directedness/vertex-count deterministically, checks the returned JSON shape, and propagates the native version string. If `VELOGRAPHX_LAGRAPH_BFS_BIN` is missing or invalid, it fails instead of substituting another implementation.

## GAP Benchmark Suite

Build GAPBS on the same target machine and provide a BFS driver implementing the same common runner contract. The stock GAP binaries may use their own input and output conventions, so the environment-specific driver is responsible for translating those conventions while preserving the actual GAP BFS execution.

```bash
export VELOGRAPHX_GAP_BFS_BIN=/absolute/path/to/gap_bfs_runner
python tools/competitor_benchmark.py \
  --dataset /absolute/path/to/graph.edgelist \
  --framework external \
  --external-command "python tools/native_competitors/gap_wrapper.py" \
  --external-name gap \
  --repeat 5 \
  --output results/gap-bfs.json
```

## Reproducibility requirements

For publication-quality comparisons, use the identical edge list, source vertex, directedness semantics, repetition policy, CPU placement, thread count, and warm-up policy for every engine. Record native repository revision or release, compiler and flags, dependency versions, CPU model, NUMA topology, memory size, kernel/OS, and any thread-affinity variables. Do not compare a converted/preprocessed graph for one engine against a different logical graph for another.

The repository's deterministic `tools/native_competitors/mock_native_bfs_runner.py` exists only to test the wrapper contract. Its timings are not competitor measurements and must never be reported as LAGraph, GraphBLAS, or GAP performance.
