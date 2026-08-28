# Normalized native BFS comparison

This stage closes the semantic gap between VeloGraphX hosted engineering evidence and like-for-like native competitor evidence.

The validator `tools/validate_normalized_native_bfs.py` runs the builtin reference plus the existing external LAGraph and GAP adapter contracts and refuses to produce a normalized result unless every engine agrees on:

- dataset SHA-256;
- source vertex;
- directedness;
- vertex and edge counts;
- reachable-vertex count; and
- the SHA-256 digest of the complete BFS distance vector.

A successful run may set `normalized_cross_engine_claim: true` only for correctness normalization and same-runner engineering timing. It must continue to emit `research_claim: false`, `publication_grade: false`, and `publication_ready: false` on GitHub-hosted hardware.

## Native runner contract

Both native runners must accept:

```text
--dataset PATH --source N --vertices N [--directed]
```

and emit exactly one JSON object containing a `distances` array with one integer per vertex (`-1` for unreachable vertices) and an immutable framework version/commit identifier where available.

The existing shims are:

- `tools/native_competitors/lagraph_wrapper.py`
- `tools/native_competitors/gap_wrapper.py`

## Remaining implementation

The hosted workflow still needs native runner binaries that expose full BFS distance vectors from the pinned LAGraph/GraphBLAS and GAP implementations. Once those runners are wired in, the normalization validator becomes the mandatory correctness gate before any cross-engine timing summary is emitted.

Publication-grade competitor claims remain blocked until the same normalized contract is executed on the dedicated hardware campaign required by Issue #10.
