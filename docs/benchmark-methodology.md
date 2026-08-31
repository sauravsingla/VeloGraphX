# Benchmark Methodology

## Principles

VeloGraphX does not publish performance claims without reproducible measurements. Benchmarks must report software versions, compiler and flags, CPU model, RAM, NUMA topology, thread count, dataset preparation, warmup, repetitions and whether loading/preprocessing time is included.

The publication-level dataset and timing contract is machine-readable in [`datasets/publication-benchmark-contract.json`](../datasets/publication-benchmark-contract.json). Its requirements apply to VeloGraphX and to external systems used for direct comparison.

## Dataset selection

Dataset choice must be defensible independently of observed VeloGraphX performance. The primary suite therefore spans structurally different graph families rather than only workloads that are favorable to localized incremental repair.

Required publication coverage is:

- **Scale-free web graphs**, with `web-Google` as a canonical example.
- **Scale-free social/community graphs**, including at least one of `soc-Epinions1`, `com-LiveJournal` or `com-Orkut` at an appropriate scale.
- **Road networks**, with SNAP `roadNet-CA` as the canonical initial workload. Road graphs provide a deliberately different low-degree, high-diameter regime.
- **Synthetic Kronecker/R-MAT graphs** for controlled scalability studies. Generator parameters, scale, edge factor and random seed must be retained with every result.

A benchmark campaign may add datasets, but it must not silently remove a required family because its results are unfavorable. Dataset checksums or immutable derivation rules accompany accepted evidence.

## Kronecker scalability contract

The default controlled synthetic family follows the Graph500-style initiator `A=0.57`, `B=0.19`, `C=0.19`, `D=0.05` with edge factor `16`. Publication campaigns should attempt scales `16, 18, 20, 22, 24, 26` or a documented hardware-appropriate extension/reduction. Every run records the exact parameters and seed.

Synthetic results are not substitutes for public real-world graphs. Their purpose is controlled scalability and memory-capacity analysis.

## Largest-practical in-memory scale

Each dedicated-hardware campaign must identify the largest configured graph that completes fully in memory without swapping or OOM. Report graph size, peak RSS, installed RAM, NUMA topology, thread count, and—where practical—the next attempted scale or the reason no larger attempt was made.

If out-of-memory or external-memory behavior is studied, those results are a separate experiment class and must not be mixed with in-memory numbers.

The executable campaign is `scripts/run_capacity_campaign.py`. It generates deterministic Kronecker workloads, records `/usr/bin/time -v` peak RSS and Linux swap/OOM counters, and only establishes a capacity boundary after at least one clean in-memory success and a larger rejected attempt. The manual `Dedicated Capacity Campaign` workflow targets `[self-hosted, linux, x64, velographx-benchmark]` and invokes the campaign with `--require-boundary`.

A real workflow-dispatch probe was executed on 2026-08-31 (run `33356427001`). The hosted smoke contract completed successfully, while the `dedicated-boundary` job remained queued without a matching self-hosted runner accepting it. The run was then cancelled and the one-time dispatch probe workflow removed. This confirms that repository-side automation is ready but no usable `velographx-benchmark` runner was available for the publication-capacity measurement at that time. A queued or cancelled run is not publication evidence.

## Standard input boundary

For end-to-end comparisons, timing begins when a non-system-specific graph representation is available to the system: a standard edge list, COO, CSR, or an equivalently neutral representation agreed for that campaign.

A system may internally convert that representation into a proprietary layout, index, compressed form, snapshot representation, or auxiliary structure. That work is part of end-to-end cost unless the comparison explicitly measures a different contract.

The default end-to-end envelope includes, as applicable:

1. graph ingestion,
2. representation conversion,
3. preprocessing and index construction,
4. initial algorithm-state construction,
5. update application,
6. incremental repair or full recomputation,
7. synchronization required before the answer is usable.

Correctness/checksum validation is normally kept outside the performance envelope and reported separately so validation overhead cannot distort system timing. If a benchmark definition requires validation inside answer-ready latency, that exception must be explicit and identical across systems.

## End-to-end versus steady-state timing

End-to-end and steady-state/kernel timing are different metrics and are never presented as interchangeable.

- **End-to-end** starts at the standard input boundary and includes system-specific conversion/preprocessing.
- **Steady-state/kernel** starts only after the system is fully initialized in its preferred internal representation.

When both are useful, report both. A comparison must not use VeloGraphX end-to-end time against a competitor's preprocessed steady-state time, or vice versa.

`velographx_public_dataset_benchmark` emits loading time, per-kernel steady-state time, and preprocessing-inclusive per-kernel end-to-end time in the same record.

## Static baseline

Initial benchmarks cover graph construction, BFS, connected components, PageRank and triangle counting. Results should include vertices, directed edge entries, elapsed time and algorithm-specific work where available.

## Dynamic benchmark plan

Dynamic runs must separate graph mutation time from algorithm-state update time and total answer-ready latency. Update fractions should include approximately 0.0001%, 0.001%, 0.01%, 0.1%, 1%, 5% and 10% of edges. Each incremental result is compared with a fresh full recomputation.

For an external dynamic-system comparison, the report must state whether the timing envelope includes ingestion, representation conversion, preprocessing/index construction, initial analytics state, update application, repair/recomputation, synchronization and validation. Unavoidable semantic or timing mismatches are disclosed adjacent to the result rather than hidden in footnotes.

## Correctness

Incremental outputs are invalid benchmark results unless they match full recomputation under exact or documented floating-point tolerance. Randomized differential tests precede large performance runs. Dataset identity, update-stream identity and exactness/checksum results are retained with benchmark artifacts.

## Repetitions

Run sufficient repetitions to expose variance. Report median and at least one dispersion statistic for stable benchmark campaigns. Do not discard slow runs without a documented reason.

## Memory and hardware counters

Where available report peak RSS, bytes per edge, cache misses, branch misses, cycles, instructions, IPC, memory bandwidth and NUMA traffic. Counter collection must not be mixed with wall-clock numbers without documenting profiler overhead.

## Baselines

Use semantically comparable versions of relevant libraries and research systems. Baseline choice should favor the strongest system appropriate to the workload semantics, not merely the easiest system to beat. Do not present an optimized C++ vs naive Python comparison as a systems breakthrough.

For every direct baseline comparison, pin the external revision, preserve identical input/update streams where semantics permit, use the same thread/core contract and apply the same end-to-end timing boundary. Cases where an external baseline wins remain part of the evidence.

## Machine-readable results

Benchmark programs should emit or be convertible to JSON/CSV with algorithm, mode, input contract, vertices, edges, update fraction, threads, elapsed time, preprocessing/loading time, end-to-end time, affected vertices/edges, memory and machine metadata.

Synthetic workloads additionally record all generation parameters and seeds.

## Unmeasured results

Any report field not produced by an executed benchmark must say **Not measured yet.** A required publication experiment is not considered complete merely because its methodology has been specified.
