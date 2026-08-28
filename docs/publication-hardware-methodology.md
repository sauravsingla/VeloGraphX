# Publication-grade controlled-hardware methodology

VeloGraphX distinguishes **hosted-CI engineering evidence** from **publication-grade performance evidence**. The existing GitHub-hosted campaigns validate correctness, reproducibility contracts, benchmark plumbing, artifact schemas, and useful crossover behavior, but they do not by themselves justify claims about dedicated-hardware scalability, NUMA efficiency, or superiority over other graph systems.

`benchmarks/publication-hardware-plan.json` is the machine-readable claim gate for publication-grade experiments. The repository copy intentionally keeps `publication_ready=false` and `allow_publication_claims=false` until real controlled-hardware evidence exists.

## Hardware readiness

Publication-grade measurements require a dedicated Linux machine. Record the CPU model, socket count, physical cores, logical CPUs, SMT state, NUMA topology, installed memory, platform/memory-channel description, kernel, compiler and version, build type/flags, CPU governor or frequency policy, and affinity policy.

NUMA claims require genuine hardware with at least two NUMA nodes. A single-socket or synthetic topology may still be used for general correctness and scaling work, but it must not be used to claim cross-socket NUMA benefit.

## Measurement protocol

Run at least two warmups before timed measurements. Use at least ten measured repetitions per configuration for publication tables. Preserve raw samples, then report medians, p95 values, and an uncertainty summary. Record requested work and actual changed work for incremental campaigns.

Thread scaling must include 1, 2, 4, 8, 16, and 32 threads when supported, and should extend to all physical cores on larger machines. Affinity must be explicit and reproducible. If SMT is evaluated, report physical-core and logical-thread scaling separately.

NUMA evaluation should include local placement, interleaved placement, and a deliberately cross-node configuration. Capture locality/remote-memory events where the platform exposes reliable counters.

Hardware-counter experiments should record cycles, instructions, branches, branch misses, cache references/misses, LLC loads/misses, and memory-traffic or remote-NUMA events when supported. Missing platform events must be reported as unavailable rather than silently omitted.

## Correctness gate

Performance evidence is invalid when correctness fails. Every incremental result used in a performance comparison must match the corresponding full/reference result under the repository's exact correctness contract. Competitor results must use semantically equivalent graph interpretation and normalized inputs.

## Datasets

Use public datasets with immutable provenance and checksums. The controlled-hardware campaign should cover at least three distinct graph families and should include larger datasets than the hosted-CI smoke/evidence campaigns where memory permits. Dataset preparation, normalization, and checksums must be captured in the result bundle.

## Competitors

Comparative claims require competitors to run on the **same hardware** under recorded software versions/builds. `benchmarks/competitor-research-plan.json` is authoritative for competitor readiness. Native `SuiteSparse:GraphBLAS/LAGraph` and the `GAP Benchmark Suite` are priority CPU-system comparisons. Floating branches such as `main`, `master`, or `HEAD` are not valid publication identities.

Package-level competitors such as NetworKit, igraph, rustworkx, and NetworkX can provide additional context, but native CPU graph systems are the more important comparison for systems claims.

## Ablations

Publication evidence should isolate where speedups come from. Required ablation dimensions include incremental versus full recomputation, adaptive versus fixed policy, scalar versus SIMD, compression enabled/disabled, work stealing enabled/disabled, and NUMA policy where applicable.

## Artifacts and claims

Each experiment must produce machine-readable raw results, environment metadata, provenance, summary statistics, and a validated result bundle. The exact commit used for the run must be recorded.

Until the readiness validator can be run against a real filled execution record with the publication gate explicitly opened, README and paper text should use wording such as **engineering evidence**, **hosted-CI result**, or **controlled-hardware measurement pending**. Do not convert hosted-CI speedups into publication-grade scalability or superiority claims.

The eventual execution of this plan is tracked separately from the contract itself so that the repository never fabricates dedicated-hardware evidence merely to satisfy a checklist.
