# Selector v2 generalization results

Status: **post-observation record; H4/H5 are no longer unseen after this campaign**.

Campaign head observed: `cd0deebde8ed4805184c1b1d8db6eb1260ea9008`.
Base `main` before selector-v2 work: `d6ed5b02e910864efaab41ba338b87fb9a414585`.
Workflow run: `34837620370`.

All numbers below are hosted GitHub Actions engineering evidence only. `research_claim=false` remains mandatory.

## Scientific outcome

Selector v2 produced a clear split:

- **Exact triangle transfer succeeded strongly on a fresh structurally different holdout.** The earlier fixed-threshold 1024-edge failure disappeared on both the seen development diagnostic and H5.
- **BFS v2 did not generalize sufficiently on H4.** Exactness and hidden-fallback behavior stayed perfect, but oracle regret and wrong-arm tails exceeded the preregistered gates.

The failed BFS gates are retained. H4 must not be reused as unseen evidence. Any BFS selector redesign informed by H4 requires a new preregistered holdout before another unseen-generalization claim.

## Seen development diagnostics

These workloads were already observed in PR #72 and are development evidence only.

### BFS — prior road/small-world family

Aggregate over 9 regimes / 243 batches:

- exactness: 100%
- internal fallback fraction: 0%
- mean oracle regret: **2.331%**
- median regret: 0%
- p95 regret: **15.177%**
- p99 regret: **39.782%**
- maximum regret: **84.553%**
- wrong-arm rate: **4.115%**
- mean selector decision cost: about **0.113 us**

The large-batch sparse-road cases remain the main weakness; for example, root 18528 / batch 2048 had 32.58% mean regret and 75% wrong-arm rate in this hosted run.

### Exact triangles — prior clustered family

Aggregate over 3 regimes / 84 batches:

- exactness: 100%
- internal fallback fraction: 0%
- mean oracle regret: **2.372%**
- median regret: 0%
- p95 regret: **17.167%**
- p99 regret: **21.980%**
- maximum regret: **23.949%**
- wrong-arm rate: **0%**

The previously severe 1024-edge regime was corrected on this seen diagnostic:

- mean regret: **0.057%**
- p95 regret: **0.195%**
- maximum regret: **0.229%**
- wrong-arm rate: **0%**

## Fresh H4 BFS holdout — preregistered gate failed

Generator: `chain-hubs-directed`, seed `2026091501`.

Identity:

- vertices: 32,768
- edges: 71,136
- roots: 0, 16,384, 32,767
- SHA-256: `4278e3cf0ba460fd485814e5e4133b09c66f87d76d931772b332625c921537cc`

Aggregate over 18 files / 222 batches:

- exactness: **100%**
- internal fallback fraction: **0%**
- mean oracle regret: **18.394%** — preregistered gate failed
- median regret: **1.442%**
- p95 regret: **83.022%** — gate failed
- p99 regret: **394.102%**
- maximum regret: **475.614%** — gate failed
- wrong-arm rate: **18.468%** — gate failed
- full-choice fraction: **10.811%**
- mean selector decision cost: about **0.146 us**

Dominant failure regimes included large batches at roots 0 and 16,384, plus a severe endpoint-root tail at root 32,767 / batch 128. This shows that uncertainty-separated historical arm costs alone are not sufficient to predict dynamic BFS repair cost across root/topology states.

**Conclusion:** `publication-preflight-v2` is not accepted as a general BFS selector based on H4. H4 is now development evidence.

## Fresh H5 exact-triangle holdout — preregistered gates passed

Generator: `block-community-undirected`, seed `2026091501`.

Identity:

- vertices: 8,192
- logical edges: 35,456
- SHA-256: `b98a7a9caffe016f103ed1ad7294f213266b6c63422c4f7b87b72cc4363ab039`

Aggregate over 6 files / 148 batches:

- exactness: **100%**
- internal fallback fraction: **0%**
- mean oracle regret: **3.162%**
- median regret: 0%
- p95 regret: **17.463%**
- p99 regret: **30.672%**
- maximum regret: **39.947%**
- wrong-arm rate: **0%**
- full-choice fraction: 0%
- mean selector decision cost: about **0.134 us**

All preregistered H5 gates passed comfortably. The triangle v2 policy is therefore frozen for subsequent work; it should not be retuned using H5 merely to improve already-good hosted numbers.

## Next research step

Only BFS proceeds to a new selector iteration. The next design may use H4 and the earlier road family as development evidence, but it must be evaluated on a **new H6 preregistered holdout** with a different topology generator and seed.

The next BFS design should target the failure mechanism rather than add another fixed threshold. Useful pre-repair signals include batch-local BFS-level structure, touched endpoint/root-state summaries, parent-edge deletion pressure, insertion-to-unreachable pressure, previous affected work, and uncertainty-aware historical costs. Cold-start behavior must also avoid spending an entire short regime on the wrong arm.

Dedicated controlled hardware, same-machine external systems, NUMA/counters where claimed, and independent reproduction remain separate publication gates.
