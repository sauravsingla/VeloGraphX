# Same-machine three-system dynamic BFS campaign

This campaign compares VeloGraphX, NetworKit `DynBFS`, and the official
RisGraph `bfs_inc_batch` program on the same hosted runner. It closes the old
cross-run comparability gap: absolute timings from the older standalone
RisGraph and NetworKit artifacts must not be combined into a three-system
table.

## Scope

The workflow covers a scale-free web graph (`web-Google`), a directed social
graph (`soc-Epinions1`), a road graph (`roadNet-CA`), a deterministic
Graph500-style R-MAT graph, and a larger social graph (`com-LiveJournal`). The
complete update sweep is `0.0001%`, `0.001%`, `0.01%`, `0.1%`, `1%`, `5%`, and
`10%` of the fixed base graph.

For every fraction, the runner uses the first 80% of the edge stream as the
base graph and constructs exactly one update batch. The batch adds the next
source-order window and removes an equally sized oldest source-order window.
This bounded one-batch design keeps the base graph identical across fractions
and avoids turning the smallest fractions into millions of sequential batches.
Reciprocal undirected streams use even base and batch boundaries.

Roots are fixed before any timed execution. Selection uses only initial and
maximum-fraction final out-degree, never benchmark timing or outcome data. The
large LiveJournal job uses one root and one repetition because of hosted-runner
resource limits; the other families use three roots and three paired
repetitions.

## Matched contract

All three native programs receive the same text-derived binary edge stream,
root, exact imported-edge boundary, update size, machine, and one-thread
configuration. The timed envelope is graph mutation plus incremental answer
maintenance. Parsing, initial graph construction, and fresh correctness BFS
are outside that envelope.

Every sample is rejected unless:

* VeloGraphX incremental BFS and its historical full-recompute policy match an
  independent fresh BFS;
* NetworKit `DynBFS` matches a fresh NetworKit BFS after the batch;
* RisGraph incremental output matches its fresh recomputation; and
* all three systems emit the same BFS layer histogram.

The artifact records means, standard deviations, raw samples, repair/full
recompute ratios, competitor ratios, dataset normalization, immutable system
revisions, environment details, and the RisGraph build-only compatibility
patch. It also runs the frozen `scale-conditioned-selector-owned-v3` policy at
every dataset/root/fraction, retaining the choice and reason, selector setup
and decision cost, all-policy exactness, always-incremental and always-full
times, and oracle-relative regret. Competitor wins and selector losses remain
in the output.

## Evidence boundary

GitHub-hosted results are engineering evidence (`research_claim: false`), not
publication-grade controlled-hardware evidence. They establish executable
same-machine methodology and reveal crossover behavior, but final paper tables
still require rerunning the unchanged workflow contract on a dedicated pinned
machine with frequency control and hardware counters.
