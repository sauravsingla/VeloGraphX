# Contributing to VeloGraphX

Contributions are welcome. Keep changes small, measurable and correctness-first.

## New contributor? Start here

These starter issues are intentionally bounded and exercise real project surfaces without requiring large datasets or benchmark campaigns:

- [#98 — Add a minimal Python dynamic-update example alongside the C++ examples](https://github.com/sauravsingla/VeloGraphX/issues/98)
- [#99 — Add a tiny exactness smoke example for incremental BFS](https://github.com/sauravsingla/VeloGraphX/issues/99)
- [#100 — Add an examples index with C++ and Python run instructions](https://github.com/sauravsingla/VeloGraphX/issues/100)

Comment on the issue if you want to coordinate before starting. For concrete technical questions that do not yet belong in an issue, use [GitHub Discussions](https://github.com/sauravsingla/VeloGraphX/discussions) — useful topics include dataset reproduction, dynamic-update workloads, graph-algorithm correctness, Python/C++ interoperability and hardware-specific results. Security reports should still follow `SECURITY.md`.

## Development flow

1. Build with CMake and run `ctest`.
2. Add tests for behavioral changes.
3. For performance changes, provide a reproducible benchmark and avoid unsupported speed claims.
4. For proposed research novelty, update `docs/research/novelty-ledger.md` with closest prior work and a falsifiable experiment.
5. Prefer portable scalar correctness before architecture-specific optimization.

## Security hygiene

Do not commit credentials, API keys, tokens, private keys, proprietary datasets, personally identifiable information, or other sensitive material. Use synthetic or public fixtures for tests and examples. Report suspected vulnerabilities privately as described in [SECURITY.md](SECURITY.md) rather than opening a public issue with exploit details.

## Coding principles

Use modern C++20, RAII, explicit ownership and clear subsystem boundaries. Avoid per-edge heap allocation, unnecessary virtual dispatch and premature lock-free complexity in hot paths.
