# Benchmark Methodology

## Principles

VeloGraphX does not publish performance claims without reproducible measurements. Benchmarks must report software versions, compiler and flags, CPU model, RAM, NUMA topology, thread count, dataset preparation, warmup, repetitions and whether loading/preprocessing time is included.

## Static baseline

Initial benchmarks cover graph construction, BFS, connected components, PageRank and triangle counting. Results should include vertices, directed edge entries, elapsed time and algorithm-specific work where available.

## Dynamic benchmark plan

Future runs must separate graph mutation time from algorithm-state update time and total end-to-end latency. Update fractions should include approximately 0.0001%, 0.001%, 0.01%, 0.1%, 1%, 5% and 10% of edges. Each incremental result is compared with a fresh full recomputation.

## Correctness

Incremental outputs are invalid benchmark results unless they match full recomputation under exact or documented floating-point tolerance. Randomized differential tests precede large performance runs.

## Repetitions

Run sufficient repetitions to expose variance. Report median and at least one dispersion statistic for stable benchmark campaigns. Do not discard slow runs without a documented reason.

## Memory and hardware counters

Where available report peak RSS, bytes per edge, cache misses, branch misses, cycles, instructions, IPC, memory bandwidth and NUMA traffic. Counter collection must not be mixed with wall-clock numbers without documenting profiler overhead.

## Baselines

Use semantically comparable versions of relevant libraries such as GAPBS, SuiteSparse:GraphBLAS/LAGraph, NetworKit, igraph, rustworkx and NetworkX when useful. Do not present an optimized C++ vs naive Python comparison as a systems breakthrough.

## Machine-readable results

Benchmark programs should emit or be convertible to JSON/CSV with algorithm, mode, vertices, edges, update fraction, threads, elapsed time, affected vertices/edges, memory and machine metadata.

## Unmeasured results

Any report field not produced by an executed benchmark must say **Not measured yet.**