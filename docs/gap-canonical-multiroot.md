# Canonical GAP multiroot campaign

The Davis-style hosted native comparison is useful controlled engineering evidence, but its synthetic 32K-vertex graph is not a substitute for the GAP Benchmark Suite corpus. This campaign adds an explicit path for the official GAP v1.5 datasets and multiple source roots while keeping correctness and timing scopes aligned.

## Canonical corpus

The dataset names and recipes are taken directly from GAP v1.5 `benchmark/bench.mk`:

| Dataset | GAP v1.5 recipe/source |
| --- | --- |
| `twitter` | ANLAB-KAIST `twitter_rv.net` trace |
| `web` | SuiteSparse / LAW `sk-2005` matrix |
| `road` | DIMACS9 `USA-road-d.USA.gr` |
| `kron` | `converter -g27 -k16` |
| `urand` | `converter -u27 -k16` |

`tools/materialize_gap_canonical.sh` mirrors those definitions and emits plain `.el` and `.wel` files. Both VeloGraphX and GAP then consume the **same materialised bytes**. The script writes SHA-256 checksums and metadata identifying GAP v1.5 as the recipe source.

A reduced graph must never be labelled canonical. The pull-request CI job deliberately uses `-g12 -k8` only to validate the runner and marks its artifact `canonical_dataset_result:false`. Exact canonical generation remains `-g27 -k16` / `-u27 -k16` for the synthetic GAP datasets.

## Multiple roots

`tools/run_gap_canonical_multiroot.py` requires at least two roots and supports an explicit comma-separated root set. The workflow default is:

```text
0,1,2,3,7,15,31,63
```

For every root and requested thread count, the runner executes BFS and weighted SSSP in both VeloGraphX and GAP. A larger recorded source set can be supplied without changing the runner; the root list is embedded in the output artifact.

## Semantics

The canonical workflow treats `twitter`, `web`, and `road` as directed inputs and `kron` / `urand` as undirected inputs. The VeloGraphX native benchmark therefore exposes `bfs-directed` and `sssp-directed` modes in addition to the existing undirected modes.

GAP's SSSP delta follows its v1.5 benchmark contract: `50000` for `road`, `2` for the other canonical datasets.

## Fair timing and correctness

For each root/thread pair:

- VeloGraphX reports its internal kernel timer with graph construction outside the timed region.
- GAP reports `Trial Time`, with file loading and graph construction outside that kernel timer.
- `OMP_PLACES=cores`, `OMP_PROC_BIND=spread`, and `OMP_DYNAMIC=FALSE` are set for both systems.
- VeloGraphX must pass its exact internal oracle check on every repetition.
- GAP must emit `Verification: PASS` on every repetition.
- file SHA-256 values, roots, thread counts, repetitions, GAP revision and per-root trial samples are retained in JSON.

The campaign does not mix dataset download/materialisation time into algorithm timing.

## Workflows

`.github/workflows/gap-canonical-multiroot.yml` has two deliberately separate paths:

1. **Contract smoke on pull requests/pushes.** A small generated graph checks that the multiroot runner, directed/weighted plumbing, GAP verification parser, and VeloGraphX correctness gates work. Its metadata explicitly says it is not canonical evidence.
2. **Exact canonical workflow dispatch.** The selected official GAP v1.5 dataset is materialised with the exact upstream recipe and then run through the same multiroot protocol.

The separation prevents a small hosted-CI graph from being accidentally presented as a canonical GAP result. Full canonical artifacts should be cited only after the exact workflow has completed successfully for the relevant dataset.

## Example

After materialising a canonical graph and building both projects:

```bash
python tools/run_gap_canonical_multiroot.py \
  --output artifacts/gap-canonical/road.result.json \
  --dataset-name road \
  --edge-list artifacts/gap-canonical/road.el \
  --weighted-edge-list artifacts/gap-canonical/road.wel \
  --velographx build/velographx_native_baseline_benchmark \
  --gap-bfs external/gapbs/bfs \
  --gap-sssp external/gapbs/sssp \
  --roots 0,1,2,3,7,15,31,63 \
  --threads 1,2,4 \
  --repeat 5 \
  --sssp-delta 50000 \
  --gap-revision v1.5 \
  --directed
```

This is the external-validity complement to the smaller controlled Davis-style hosted campaign; it does not replace that campaign's incremental update experiments.
