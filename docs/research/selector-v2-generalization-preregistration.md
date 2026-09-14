# Selector v2 generalization preregistration

Status: **frozen before selector-v2 measurements are observed**.

This campaign follows the retained post-PR71 results. The previously observed `road-smallworld` BFS and `clustered-undirected` triangle workloads are now **development evidence only** and may be used to diagnose whether the known failure modes were addressed. They must never again be described as unseen.

## Frozen design before measurement

Base `main` commit before this development line: `d6ed5b02e910864efaab41ba338b87fb9a414585`.

New selector implementations:

- BFS: `publication-preflight-v2` in `benchmarks/publication_policy_bfs_v2.cpp`.
- Triangles: `publication-preflight-triangle-v2` in `benchmarks/publication_policy_triangles_v2.cpp`.

The design change is intentionally narrow: remove topology-specific / fixed update-fraction forced-full rules and choose full recomputation only when online measured cost predictions separate beyond uncertainty bands. The first missing incremental-arm observation is obtained by an incremental probe because initial construction already provides a full-arm observation.

No correctness semantics change. Both arms remain exact and oracle regret remains defined against the same-run `min(always_incremental, always_full)` oracle with full winning ties.

## Seen development diagnostics

The following are explicitly **not holdout** after PR #72:

- BFS: `road-smallworld`, seed `20260914`, roots `0,18528,36863`, batches `128,512,2048`.
- Triangles: `clustered-undirected`, seed `20260914`, batches `64,256,1024`.

These may be used only to verify that v2 addresses the already-known sparse/high-diameter BFS misclassification and the 1024-edge triangle forced-full failure.

## Fresh holdout H4 — BFS

Generator: `selector-v2-holdout-v1`, mode `chain-hubs-directed`.

- seed: `2026091501`
- vertices: `32768`
- import rate: `0.95`
- roots: `0,16384,32767`
- batch sizes: `128,512,2048`
- repetitions: `2`
- hosted runner: Ubuntu 22.04, one OpenMP thread

This family is structurally distinct from the prior road/grid holdout: bidirectional chain backbone, sparse hub fan-out and deterministic long directed edges.

## Fresh holdout H5 — exact triangles

Generator: `selector-v2-holdout-v1`, mode `block-community-undirected`.

- seed: `2026091501`
- vertices: `8192`
- import rate: `0.90`
- batch sizes: `64,256,1024`
- repetitions: `2`
- hosted runner: Ubuntu 22.04, one OpenMP thread

This family is structurally distinct from the prior ring-lattice clustered holdout: block communities, anchor spokes and sparse inter-community bridges.

## Predeclared evaluation metrics

For BFS and triangles report, without suppressing unfavorable regimes:

- exactness;
- mean / median / p95 / p99 / maximum oracle regret;
- wrong-arm rate;
- full-choice fraction;
- selector decision cost;
- internal repair-to-full fallback fraction;
- per-regime metrics.

## Predeclared hosted engineering gates

These are engineering/generalization gates, **not publication-grade performance claims**.

BFS H4:

- exactness = 100%;
- internal fallback fraction = 0;
- mean oracle regret <= 8%;
- p95 oracle regret <= 40%;
- maximum oracle regret <= 100%;
- wrong-arm rate <= 8%.

Triangle H5:

- exactness = 100%;
- internal fallback fraction = 0;
- mean oracle regret <= 20%;
- p95 oracle regret <= 75%;
- maximum oracle regret <= 200%;
- wrong-arm rate <= 10%.

If a gate fails, the result is retained. Any selector change informed by H4/H5 makes those workloads development evidence and requires another preregistered holdout for a new generalization claim.

## Claim boundary

`research_claim=false` for all hosted results. Dedicated controlled hardware, same-machine external systems, NUMA/counters where claimed, and independent reproduction remain separate publication gates.
