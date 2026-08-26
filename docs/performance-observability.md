# Performance observability

VeloGraphX treats graph performance as a memory-hierarchy and work-efficiency problem, not only an elapsed-time problem. On Linux, `scripts/perf_profile.sh` provides a reproducible wrapper around `perf stat` for benchmark binaries.

The harness attempts to collect cycles, instructions, branches, branch misses, cache references, cache misses, L1 data-cache loads and L1 data-cache load misses. It also emits a JSON metadata sidecar containing platform, kernel, architecture, logical CPU count, compiler text and the Git commit SHA. Counter availability varies by processor and by `perf_event_paranoid`/container policy, so unavailable counters are reported as an environment limitation and do not make the benchmark harness fail.

Run, for example:

```bash
bash scripts/perf_profile.sh velographx_dynamic_benchmark
bash scripts/perf_profile.sh velographx_intersection_benchmark
```

Results are written to `perf-results/` by default. Do not commit machine-specific performance output as universal project results. Published numbers must record the machine, compiler/build flags, graph/dataset, thread count, repetitions and workload configuration.

For NUMA studies, combine these counters with `numactl --hardware`, `numastat`, and platform-supported uncore/memory events where available. Multi-socket NUMA traffic and memory-bandwidth claims remain **Not measured** until run on suitable hardware; GitHub-hosted CI is used only to validate portability and correctness, not to manufacture hardware-performance claims.
