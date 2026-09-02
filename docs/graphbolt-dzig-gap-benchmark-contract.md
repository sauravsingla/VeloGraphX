# GraphBolt/DZiG and GAPBS publication contract

This campaign is an executable contract, not a performance claim. Hosted CI
validates workload identity, provenance, CPU planning, environment capture and
measurement gates. Native comparative numbers are publication evidence only
when produced on a controlled dedicated runner with pinned competitor commits.

## Neutral workload

`tools/prepare_graphbolt_workload.py` removes blank/comment rows, validates a
simple directed edge set, assigns every normalized row a seeded SHA-256 shuffle
key, externally merges those keys, and splits the result into `initial.edges`,
`insertions.edges`, and valid `deletions.edges`. `metadata.json` retains the
source and output checksums, seed, algorithm, row counts and validity contract.
The external merge bounds memory use; changing `--chunk-rows` must not change
the output.

Adapters for VeloGraphX and GraphBolt/DZiG must consume these exact files. An
adapter may convert them, but must retain conversion commands, checksums and the
pinned source revision. Invalid, duplicate or differently interpreted updates
must be counted and reported; silently dropping operations invalidates a run.

## CPU and memory policy

`tools/configure_publication_cpu.py` selects one allowed logical CPU from each
physical `(socket, core)` and emits `OMP_NUM_THREADS`, `OMP_DYNAMIC`,
`OMP_PROC_BIND`, and `GOMP_CPU_AFFINITY`. The generated JSON, the full output of
`tools/capture_benchmark_environment.py`, and `lscpu` belong in every artifact.
On the dedicated runner, execute under an explicitly recorded `numactl` policy.
Use parallel first-touch initialization after affinity is established. Record
THP state; do not change host THP configuration from an unprivileged workflow.

## Timing and correctness

Each batch records graph-update, repair/compute, and answer-ready times; batch
size/fraction; valid and invalid operation counts; affected vertices/edges (or
the closest native work counter); fresh-recompute time; and exactness/tolerance
status. A performance row is discarded on correctness failure.

Cold end-to-end trials and warm steady-state kernel trials are separate series.
Raw samples are retained. `tools/validate_publication_measurements.py` reports
median, mean, standard deviation, range and flags medians below the configured
noise floor. Publication jobs require multiple effective thread counts and must
not hide configurations in which GAPBS or GraphBolt/DZiG wins.

## Intentionally blocked acceptance items

Native GraphBolt/DZiG checkout/build commands and output parsing must be added
only against a verified public artifact revision and its actual license/CLI;
the repository must not invent an adapter contract. Near-memory-capacity data
and first-touch/NUMA performance claims require the dedicated runner. The
workflow therefore marks these inputs as required for a native run and never
substitutes hosted-CI measurements.
