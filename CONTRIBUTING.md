# Contributing to VeloGraphX

Contributions are welcome. Keep changes small, measurable and correctness-first.

## Development flow

1. Build with CMake and run `ctest`.
2. Add tests for behavioral changes.
3. For performance changes, provide a reproducible benchmark and avoid unsupported speed claims.
4. For proposed research novelty, update `docs/research/novelty-ledger.md` with closest prior work and a falsifiable experiment.
5. Prefer portable scalar correctness before architecture-specific optimization.

## Coding principles

Use modern C++20, RAII, explicit ownership and clear subsystem boundaries. Avoid per-edge heap allocation, unnecessary virtual dispatch and premature lock-free complexity in hot paths.
