# BFS selector v3 H6 results

Status: **post-observation retained result**. The preregistration in `bfs-selector-v3-h6-preregistration.md` was committed before the first H6 execution and is not modified by this result record.

## Provenance

- frozen selector: `publication-preflight-bfs-v3-cascade-reuse2`
- preregistration commit: `6735f6b31a9c30790128d6404016aecf96f7c4dc`
- H6 execution workflow commit: `11e80d4f71e7c2fa6302a100ad660c1edb0d7264`
- GitHub Actions run: `34872245950`
- H6 artifact: `bfs-selector-v3-h6-holdout`, artifact ID `10359727252`
- artifact ZIP SHA256: `e71704c9a522d1a2fefe6a4abcde471e2dfbfdc547687087db8ba8648f20e88c`
- graph family: `layered-diamond-directed`
- graph SHA256: `f97dbd6c0fa63ee608a063ab608e11276435ce5e4e1bd85c8ba2af713ba5b7d1`
- graph: 49,152 vertices, 164,336 directed edges
- roots: `0,24661,49151`
- batches: `128,512,2048`
- repetitions: `3`
- aggregate adaptive samples: `783`
- `research_claim=false`

The workflow verified all preregistered candidate Git-blob identities and the expected graph SHA256 before any benchmark measurement.

### Post-observation reproducibility audit

A cleanup review after H6 found one transitive Python import dependency that was present in the H6 execution commit but was not separately enumerated in the preregistration's blob checklist: `tools/prepare_bfs_selector_v3_candidate.py` imports `tools/prepare_bfs_selector_v3_cascade.py` at module load time. The H6/reuse2 path calls `add_reachfix()` from that module and does **not** invoke the legacy cascade transform, but the import must resolve for deterministic replay. The exact dependency present during H6 has Git blob `702237519114b5f193f76bff6f11c47173126758`; it is retained unchanged. This post-observation audit does not modify the frozen candidate, generator, H6 measurements or acceptance gates.

## Frozen-gate outcome

**H6 passes every preregistered hosted generalization gate.**

| Metric | Frozen gate | Observed |
| --- | ---: | ---: |
| Exactness | 100% | 100% |
| Internal repair-to-full fallback | 0% | 0% |
| Mean oracle regret | <= 10% | 5.3071% |
| p95 oracle regret | <= 50% | 22.8439% |
| Wrong-arm rate | <= 12% | 0.6386% |
| Reusable preflight exercised | yes | yes |

Mandatory reported, non-gated tail metrics:

- median regret: `0.009280902917052686` = 0.9281%;
- p99 regret: `0.4855178123451993` = 48.5518%;
- maximum regret: `0.7714649048440377` = 77.1465% excess, i.e. selected latency about 1.7715x the same-run oracle on the worst observed batch;
- full-choice fraction: `0.011494252873563218` = 1.1494%;
- mean selector decision cost: `121.61641890166028` microseconds.

Reusable-preflight audit:

- cascade previews evaluated: `783`;
- prepared previews reused by incremental execution: `774`;
- bounded previews exceeded: `9`;
- selector-owned cascade full recomputations: `9`;
- internal repair-to-full fallbacks: `0`.

## Interpretation

The H6 result is positive generalization evidence for the **BFS** reusable pre-repair execution policy. On a graph family not used to tune `cascade-reuse2`, the candidate remained exact, preserved selector-owned recomputation with no hidden repair-to-full fallback, and passed the frozen mean, p95 and wrong-arm gates without changing thresholds after observation.

This is materially stronger than the earlier post-PR71 unseen BFS result because the holdout was frozen after the selector redesign and the candidate passed its predeclared gates. It does **not** establish a universal graph-algorithm selector: the retained triangle failure from the prior campaign still stands, and no triangle claim is changed by H6.

The aggregate result should not hide per-regime variation. In the five-sample root-0 / batch-2048 regimes, wrong-arm rates reached 20-40% in two repetitions even though aggregate wrong-arm rate was only 0.6386%. Separately, one root-0 / batch-128 repetition had p95 regret about 56.86% with zero wrong-arm decisions, illustrating hosted-run same-arm timing variability. These regimes are retained rather than tuned away.

## Claim boundary

H6 is hosted-CI engineering/generalization evidence, not publication-grade absolute performance. The current candidate may now be described as having passed a preregistered unseen **BFS** holdout under the frozen hosted gates. It should not be described as publication-ready solely from H6.

Publication-grade remaining work is unchanged: dedicated controlled hardware, same-machine native external-system baselines where semantics align, broader public datasets / roots / seeds, hardware or NUMA evidence where claimed, and independent reproduction or adoption.

Because H6 has now been observed, any future selector change informed by H6 converts this workload to development evidence and requires a newly preregistered H7 (or later) holdout before another unseen-generalization claim.
