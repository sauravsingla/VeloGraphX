# Examples

Compact index of the runnable examples in this directory. Each entry states what
the example shows, how to build and run it, what it prints, and whether it needs
any external data.

These examples use only the inline, hard-coded graphs in the source files — no
datasets, downloads, or external data are required. Benchmark and research
material lives under [`../benchmarks/`](../benchmarks/) and is deliberately not
reproduced here.

## C++ examples

Both examples build as part of the default CMake configuration (no extra
options needed). From a repository checkout:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### `basic.cpp` — static graph analytics

- **Purpose:** the minimal static-API tour. Builds a small fixed graph, then
  runs two one-shot analytics on it: BFS distances from a source vertex and a
  global triangle count.
- **CMake target:** `velographx_example`
- **Run:**
  ```bash
  ./build/velographx_example
  ```
- **Expected output:** one line with the graph's vertex count and its triangle
  count, then one line per vertex giving the BFS distance from the source
  (vertex `0`) to that vertex. This exercises `CsrGraph`, `bfs_distances`, and
  `triangle_count`.
- **External data:** none.

### `dynamic_transactions.cpp` — dynamic graph with incremental analytics

- **Purpose:** the minimal dynamic-API tour. Seeds a `DynamicGraph`, applies an
  initial batch, then applies a second update batch through an
  `IncrementalTriangleCount` maintainer and asks the runtime to pick and explain
  an execution plan for that update.
- **CMake target:** `velographx_dynamic_example`
- **Run:**
  ```bash
  ./build/velographx_dynamic_example
  ```
- **Expected output:** a single line with the current maintained triangle count
  plus a short explanation of which execution plan the selector chose for the
  update (localized repair vs. recomputation). This exercises `DynamicGraph`,
  `IncrementalTriangleCount`, and the `execution_plan` selector.
- **External data:** none.

## Python examples

There is no Python example checked in under `examples/` at the time this index
was written. The Python path is covered by the package itself: install
`velographx` from PyPI and use the short API sample in the
[repository README](../README.md) ("Try it in 30 seconds"). See
[`../python/README.md`](../python/README.md) for install options (PyPI, local
`pip install .`, editable install, and the manual CMake build) and for the
tested interoperability adapters (NumPy, SciPy, Arrow).

## Related

- [Repository README](../README.md) — getting started, installation, and
  benchmark-evidence boundaries.
- [`../python/README.md`](../python/README.md) — Python bindings.
- [`../benchmarks/README.md`](../benchmarks/README.md) — benchmark harness and
  campaigns (research surface, kept separate from these quick examples).
