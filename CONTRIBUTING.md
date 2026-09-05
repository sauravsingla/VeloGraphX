# Contributing to VeloGraphX

Contributions are welcome. Keep changes small, measurable and correctness-first.

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
