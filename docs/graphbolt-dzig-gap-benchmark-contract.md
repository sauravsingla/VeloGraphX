# GraphBolt/DZiG and GAPBS benchmark contract

This campaign is an executable evidence contract, not a publication-grade performance claim. Hosted CI now executes the pinned official GraphBolt artifact natively inside its required legacy toolchain, checks equivalent mutation streams and exact outputs, and retains raw timing evidence. Hosted measurements remain **engineering evidence** (`publication_grade=false`, `research_claim=false`) until reproduced on controlled dedicated hardware.

## Neutral workload

`tools/prepare_graphbolt_workload.py` removes blank/comment rows, validates a simple directed edge set, assigns every normalized row a seeded SHA-256 shuffle key, externally merges those keys, and splits the result into `initial.edges`, `insertions.edges`, valid `deletions.edges`, and `graphbolt.stream`. It accepts either an initial-graph fraction or a target operation fraction and records requested and actual operation fractions.

Metadata retains source/output checksums, seed, shuffle algorithm, row counts, validity semantics and GraphBolt-native stream requirements. The external merge bounds memory use; changing `--chunk-rows` must not change the deterministic output.

GraphBolt consumes alternating `a source target` / `d source target` rows with `-fixedBatchSize`, `-enforceEdgeValidity`, and `-simple`; `-nEdges` counts individual edge operations. VeloGraphX consumes the same initial edge set and exact stream. Conversion steps, pinned revisions and generated final graphs are retained in artifacts.

## Hosted dataset matrix

`datasets/graphbolt-hosted-matrix.json` defines four hosted families:

- `web-Google` — scale-free web graph;
- `soc-Epinions1` — scale-free social graph;
- `roadNet-CA` — road network;
- deterministic `rmat-scale17` — Kronecker/R-MAT family.

Real source archives are checksum verified. `tools/prepare_hosted_graph_dataset.py` selects a deterministic bounded edge sample using seeded SHA-256 edge keys, remaps vertex IDs contiguously, records the prepared-graph checksum, and chooses three deterministic high-outdegree roots. Undirected sources are expanded symmetrically before the directed hosted representation is sampled. The bounded hosted samples are for CI engineering evidence only; full-source and near-memory-capacity runs remain dedicated-runner work.

The expanded hosted matrix sweeps approximately **0.001%, 0.01%, 0.1%, 1%, 5%, and 10%** graph operations per initial edge, across three roots and seven fresh-process samples per root/regime/system.

## Timing, statistics and correctness

The comparable dynamic timing envelope is:

- **VeloGraphX:** graph mutation + incremental answer maintenance; fresh verification excluded;
- **GraphBolt/DZiG:** graph mutation + incremental answer maintenance; stream-reading time excluded and retained separately;
- **GAPBS:** post-update fresh BFS kernel on an already-materialized final graph; graph mutation/loading excluded, so this is a static reference rather than a directly comparable dynamic-system envelope.

`tools/summarize_hosted_graphbolt_matrix.py` retains raw samples and reports median, mean, MAD, population standard deviation, range, CV, throughput, latency/update, VeloGraphX affected vertices, GraphBolt phase timings, and GraphBolt native work counters when the artifact emits them. Rows are flagged rather than deleted when any system median is below the 1 ms hosted noise floor. Competitor wins remain in the artifact and summary.

GraphBolt's legacy artifact does not emit every optional work counter for every native BFS configuration. Missing `Affected` add/delete counts or `EDGEWORK` are therefore represented explicitly as unavailable/null; they are never invented as zero. When those counters are emitted, consistency is checked. Independently of optional counters, the frozen mutation stream itself remains checksum/row-count pinned and every GraphBolt final answer must pass fresh recomputation.

`tools/verify_graphbolt_bfs_output.py` independently applies the exact frozen stream and recomputes directed reachability from scratch. It compares every GraphBolt 0/1 vertex value exactly and rejects invalid operations or a different vertex domain. This is deliberately described as directed reachability, matching the official artifact's BFS output semantics rather than claiming shortest-path-distance equivalence.

VeloGraphX's neutral-stream comparator independently recomputes fresh BFS after the timed update/maintenance section and compares the complete distance vector. No dynamic performance row survives a correctness failure.

## CPU/environment evidence

Hosted jobs force one software worker (`OMP_NUM_THREADS=1` for GAPBS and `CILK_NWORKERS=1` for GraphBolt) and retain `lscpu`, logical CPU count, visible SMT state, THP state, process affinity and relevant OpenMP/Cilk environment variables. These records characterize the shared runner; they do **not** turn it into controlled hardware.

The dedicated publication campaign additionally requires one thread per selected physical core, explicit affinity, validated 1/2/4/8/16/32-thread scaling, first-touch/NUMA policy, controlled SMT/turbo/frequency policy, larger full-source graphs, and near-memory-capacity behavior. Those controls cannot be inferred from a GitHub-hosted runner.

## Pinned native systems

Official GraphBolt is pinned at `2d56f39cb17c85d624bee6a63f8fc34a8f149a36`. Hosted CI builds it in Ubuntu 18.04 with GCC/G++ 7 Cilk Plus and the artifact's mimalloc 1.6 setup, then runs the official converter and BFS application. GAPBS is pinned to v1.5 and its exact resolved commit is retained in the artifact.

The stricter controlled-hardware path remains defined by [canonical publication campaign](canonical-publication-campaign.md) and [controlled hardware execution](controlled-hardware-execution.md).
