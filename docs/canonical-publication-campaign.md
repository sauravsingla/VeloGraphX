# Canonical publication campaign

The complete controlled-hardware entry point is
`.github/workflows/canonical-publication-campaign.yml`. It composes the
previously separate real-dataset, Kronecker capacity, CPU-scaling, NUMA and
hardware-counter harnesses into one claim-gated artifact.

## Required coverage

The checksum-pinned real-dataset manifest is
`datasets/manifest.publication-canonical.json`:

| Family | Dataset | Immutable identity |
|---|---|---|
| scale-free web | `web-Google` | immutable Git source revision + SHA-256 |
| scale-free social/community | `com-LiveJournal` | canonical SNAP archive + SHA-256 |
| road network | `roadNet-CA` | canonical SNAP archive + SHA-256 |

The synthetic campaign uses the documented Graph500-style R-MAT initiator
`A=0.57, B=0.19, C=0.19, D=0.05`, edge factor 16 by default, a retained seed,
and scales `16,18,20,22,24,26`. It stops at the first rejected scale. A valid
capacity result requires both a clean in-memory success and a larger attempted
scale that fails, swaps, or encounters an OOM event.

## Measurement contract

Each real graph receives two warmups and ten retained end-to-end repetitions.
Every sample preserves raw CSV output, per-process POSIX resource usage including peak RSS, and result
signatures. The campaign fails if BFS reachability, component count, triangle
count, graph size, or PageRank sum changes across repetitions.

The dedicated phase additionally requires:

- a self-hosted runner labelled `velographx-benchmark`;
- at least 32 allocated CPUs;
- 1/2/4/8/16/32-thread cases;
- ten samples and two warmups per controlled case;
- `perf` hardware-counter collection;
- genuine NUMA local, interleaved and cross-node cases; and
- a successful Kronecker in-memory capacity boundary.

`tools/finalize_canonical_publication_campaign.py` opens the publication gate
only when every required graph family, repetition contract, Kronecker series,
capacity boundary, controlled-hardware case and hardware-counter case passes.
Hosted CI exercises the complete artifact plumbing with small fixtures but
deliberately leaves the publication gate closed.

## Manual dedicated execution

Dispatch **Canonical Publication Campaign** with `run_controlled=true`. The
workflow retains the final summary, raw samples, dataset metadata, environment,
machine topology, memory snapshots, capacity records and hardware cases for 90
days. If the configured largest R-MAT scale still fits, extend the scale list;
the gate correctly remains closed until a larger rejected attempt establishes
the capacity boundary.
