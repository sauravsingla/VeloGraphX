# Tools

Reserved for dataset converters, profiling helpers, benchmark-report generation and future calibration utilities.

Issue #41 publication helpers:

- `prepare_graphbolt_workload.py`: deterministic external shuffle/split with checksums and valid insert/delete streams.
- `configure_publication_cpu.py`: physical-core-only OpenMP/libgomp affinity plan.
- `validate_publication_measurements.py`: raw-sample statistics, noise-floor and thread-scaling gates.
