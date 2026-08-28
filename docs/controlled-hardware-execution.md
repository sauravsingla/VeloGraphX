# Controlled-hardware execution for Issue #10

This is the Phase B execution entry point for publication-grade CPU measurements. It composes the existing single-case `hardware_campaign_driver.py` into repeated thread-scaling, NUMA and hardware-counter cases while preserving the repository claim gate.

## Preconditions

Use a dedicated Linux machine. For the full campaign, the host should expose at least 32 logical/physical execution slots appropriate for the benchmark, `taskset`, `numactl`, and Linux `perf`. NUMA claims require at least two genuine NUMA nodes. Keep CPU governor/frequency, SMT policy, compiler, build flags, kernel, dataset checksums and competitor revisions fixed and recorded.

Build the benchmark in release mode before collecting measurements.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Thread scaling + counters + NUMA

Example invocation:

```bash
python3 tools/run_controlled_hardware_campaign.py \
  --threads 1,2,4,8,16,32 \
  --repeats 10 \
  --warmups 2 \
  --perf \
  --numa \
  --output controlled-hardware-results.json \
  -- ./build/velographx_benchmark_public_dataset <dataset.edgelist> 0
```

The orchestrator automatically skips requested thread counts above the detected CPU count and records that fact. It records per-sample wall time plus the complete low-level driver output. NUMA cases are only executed when at least two NUMA nodes are detected.

## Claim gate

The resulting artifact contains both `publication_ready` and `research_claim`, but the runner deliberately keeps `claim_gate.allow_publication_claims` false. A successful hardware run is necessary but not sufficient for comparative publication claims.

Before publishing performance claims, also require:

- immutable public dataset provenance and checksums;
- exact correctness validation for every compared result;
- pinned same-hardware native LAGraph/GraphBLAS and GAP executions;
- validated result bundles and environment capture;
- required ablations and update-fraction campaigns;
- repository publication-readiness validators passing on the final evidence bundle.

Hosted GitHub Actions results remain engineering evidence and must not be relabeled as controlled-hardware publication results.

## Expected evidence

For a complete Phase B package, retain the raw JSON output, environment capture, benchmark build metadata, competitor version/build records, correctness digests, raw competitor samples, public-dataset provenance and all summary artifacts. Issue #10 should remain open until those measurements are actually produced on suitable hardware and pass validation.
