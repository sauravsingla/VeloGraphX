# Native competitor adapters

VeloGraphX keeps heavyweight native competitors behind a normalized external benchmark contract for dedicated-machine execution. A separate hosted evidence workflow also builds immutable GraphBLAS/LAGraph and GAP revisions from source, runs their native BFS/SSSP programs and retains correctness-gated engineering artifacts. Hosted measurements do not replace the normalized dedicated-hardware contract below.

See [`hosted-native-competitors.md`](hosted-native-competitors.md) for the completed hosted campaign and [`gap-canonical-multiroot.md`](gap-canonical-multiroot.md) for the official GAP-dataset path.

## Contract

`tools/competitor_benchmark.py --framework external` sets these environment variables for the adapter command:

- `VELOGRAPHX_DATASET`: absolute edge-list path
- `VELOGRAPHX_ALGORITHM`: currently `bfs`
- `VELOGRAPHX_SOURCE`: source vertex
- `VELOGRAPHX_DIRECTED`: `1` or `0`
- `VELOGRAPHX_VERTICES`: vertex count

The adapter must print exactly one JSON object containing a `distances` array of length `VELOGRAPHX_VERTICES`. It may also report `framework_version`.

## SuiteSparse:GraphBLAS / LAGraph

Use `tools/native_competitors/lagraph_wrapper.py`. Set `VELOGRAPHX_LAGRAPH_BFS_BIN` to a locally built runner that accepts:

```text
--dataset PATH --source N --vertices N [--directed]
```

and prints:

```json
{"framework_version":"<local version>","distances":[0,1,-1]}
```

Example:

```bash
export VELOGRAPHX_LAGRAPH_BFS_BIN=/opt/lagraph/bin/velographx_lagraph_bfs
python tools/competitor_benchmark.py \
  --dataset /data/graph.edges \
  --framework external \
  --external-name lagraph \
  --external-command "python tools/native_competitors/lagraph_wrapper.py" \
  --repeat 5 \
  --output results/lagraph-bfs.json
```

The wrapper validates the binary path, dataset, result JSON and distance-vector length. It does not synthesize fallback results when LAGraph is absent.

## GAP Benchmark Suite

Use `tools/native_competitors/gap_wrapper.py`. Set `VELOGRAPHX_GAP_BFS_BIN` to a locally built runner with the same command-line contract:

```text
--dataset PATH --source N --vertices N [--directed]
```

Example:

```bash
export VELOGRAPHX_GAP_BFS_BIN=/opt/gapbs/bin/velographx_gap_bfs
python tools/competitor_benchmark.py \
  --dataset /data/graph.edges \
  --framework external \
  --external-name gap \
  --external-command "python tools/native_competitors/gap_wrapper.py" \
  --repeat 5 \
  --output results/gap-bfs.json
```

## Reproducibility requirements

For publication-quality comparisons, use the same checksum-verified dataset, source vertex, directedness, warm-up policy and repeat count across all engines. Record CPU model, core/thread count, memory topology, compiler/runtime versions and native framework version alongside the JSON reports. Do not use GitHub-hosted runner timings as research evidence; the hosted CI only validates the adapter contract and repository correctness.
