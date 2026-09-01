# Teseo same-algorithm storage evidence

This experiment isolates **storage/layout effects** by running the exact same VeloGraphX BFS implementation, `BasicIncrementalBFS<Graph>::recompute()`, over three graph representations: VeloGraphX `DynamicGraph`, read-optimised `CsrGraph`, and an external Teseo adapter.

The Teseo adapter is non-intrusive: the adapter graph does not expose VeloGraphX-style `vertex_count()`, `directed()`, `neighbors()`, or `for_each_neighbor()` members. It opts into the C++20 graph-access contract through ADL `vx_*` free functions. This is intended to demonstrate that a foreign representation can be used without inheritance or modifying the foreign library.

## Contract

- Teseo repository: `cwida/teseo`
- pinned Teseo commit: `2c37c2831c4d2acaaa838a86e1318363ce68c45b`
- algorithm: identical `BasicIncrementalBFS::recompute()` for all three representations
- graph semantics: undirected simple graph, dense vertex IDs, unit-weight traversal
- source: vertex `0`
- repetitions: 5; table reports medians
- timing: graph construction/loading excluded; only repeated recomputation is timed
- correctness: complete BFS distance vectors must match across all three backends
- runner: GitHub-hosted Ubuntu 22.04
- claim class: engineering evidence only, not publication-grade hardware evidence

## Results

| Vertices | Edges | `DynamicGraph` | `CsrGraph` | Teseo adapter | Exact |
| ---: | ---: | ---: | ---: | ---: | :---: |
| 8,192 | 32,768 | 120.434 µs | 57.988 µs | 4,341.401 µs | yes |
| 32,768 | 131,072 | 500.383 µs | 234.284 µs | 21,854.866 µs | yes |

On these two hosted storage-swap cases, read-optimised CSR is about **2.08×–2.14× faster** than VeloGraphX `DynamicGraph` for full BFS recomputation. With the same VeloGraphX BFS algorithm, `DynamicGraph` traversal is about **36.0×–43.7× faster** than traversal through the Teseo iterator adapter.

These numbers are **not a system-level claim that VeloGraphX is 36×–44× faster than Teseo**. They measure one fixed BFS implementation through each storage interface. Teseo's own algorithms, tuning, update throughput, concurrency and other system capabilities are outside this experiment. The result is useful specifically because the algorithm is held constant while storage is changed.

## Reproduction

```bash
gh workflow run external-teseo-storage.yml --ref main
```

Validated run: `33475389747`. Artifact: `9787994251` (`velographx-teseo-storage-swap`). The workflow pins the external commit, builds Teseo from source, runs the full VeloGraphX test suite first, executes both storage-swap sizes, requires exact output equality, and records immutable experiment metadata.

Sortledton remains a useful additional independent storage backend for future replication; it is not needed to establish that the current graph-access contract supports a real external representation.
