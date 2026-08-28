# Hosted CI baseline for Issue #10

This campaign executes the subset of the controlled-hardware benchmark plan that is meaningful on GitHub-hosted CPU runners.

It is **engineering evidence only**. It is not publication-grade performance evidence and does not open the publication claim gate.

## What the hosted baseline measures

- thread-limited benchmark runs at 1, 2, and 4 threads when the runner exposes those logical CPUs;
- five repeated samples per thread count;
- paired intersection, compression, and incremental ablation suites;
- same-runner BFS comparisons across the builtin reference, NetworkX, igraph, NetworKit, and rustworkx;
- exact normalized result-digest agreement against the builtin reference;
- a best-effort Linux `perf stat` attempt for cycles, instructions, cache misses, branches, and branch misses;
- environment metadata and raw machine-readable outputs.

If the hosted runner denies access to hardware performance counters, the campaign records that limitation rather than treating it as a benchmark failure.

## Claims that remain prohibited

Hosted results must not be described as evidence for:

- publication-grade scalability;
- stable 8/16/32+ core scaling;
- genuine multi-socket NUMA locality or remote-memory effects;
- controlled same-hardware superiority over native research systems;
- production hardware performance.

Those claims remain blocked on the dedicated-hardware phase of Issue #10.

## Why this is useful

The hosted baseline validates the campaign plumbing, correctness contracts, repeat structure, competitor normalization, artifact generation, and the ability to collect whatever hardware-sensitive metadata the runner permits. The same methodology can later be rerun on dedicated hardware without changing the evidence rules.
