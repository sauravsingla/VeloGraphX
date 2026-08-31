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

## Completed hosted execution

The completed hosted campaign on commit `906d78cdaef5043211e51a3982ff77d36d0410c4` exercised the required **1, 2, and 4 thread counts with five repetitions per count** and passed the campaign, correctness, preflight, result-bundle, and repository regression gates.

- GitHub Actions run: `33360628638`
- artifact: `9746573265` (`velographx-ci-scale-evidence`)
- artifact SHA-256: `126196803396c3ab2e8f05bffd7bf93a51e2b1f8e758948a464fba40d356b3e9`
- artifact retention: 30 days from the run
- research claim: `false`
- publication grade: `false`

This closes the hosted 1/2/4-thread execution gap identified in the earlier CI-scale campaign. It does **not** create a thread-scaling performance claim; the hosted runner is still unsuitable for publication-grade scaling conclusions.

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
