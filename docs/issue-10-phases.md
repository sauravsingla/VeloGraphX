# Issue #10 execution phases

## Phase A — hosted CI baseline

Status: feasible on GitHub-hosted runners.

Produces engineering evidence for limited thread scaling, paired ablations, same-runner Python/native-core competitor comparisons, correctness normalization, environment capture, and best-effort hardware counters. The publication claim gate remains closed.

## Phase B — dedicated controlled hardware

Status: blocked on suitable hardware.

Requires dedicated Linux hardware, 8/16/32+ cores where available, genuine NUMA/multi-socket topology for NUMA claims, stable frequency/affinity controls, hardware counters, pinned native LAGraph/GraphBLAS and GAP builds, repeated public-dataset campaigns, and validated publication result bundles.

Phase A is intentionally designed to validate the measurement machinery before Phase B. Phase A completion does not complete Issue #10.
