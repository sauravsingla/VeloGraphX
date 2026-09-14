# Post-PR71 reviewer-closure observed results

This file records the first completed outcome of the preregistered campaign in [`post-pr71-reviewer-closure.md`](post-pr71-reviewer-closure.md). It was added **after** the measurements were observed so that the preregistration file can remain unchanged.

## Immutable provenance

- Frozen base before the campaign: `559adc1c77f5deffcede9bc4aea1327bf53bc2a9`.
- Measured PR head: `36260192cb061d2d9ad4e08091cda8718653e99e`.
- GitHub Actions run: `34835332233` (`Reviewer Closure Validation`).
- Retained artifact: `reviewer-closure-validation`, artifact ID `10342919410`.
- Artifact ZIP SHA-256: `aef661b3252ebfb207f496aa3a703204bab282bcafe400748f3351567aa52fe6`.
- BFS selector: `publication-preflight-v1` (unchanged from PR #71).
- Hosted runner only: `research_claim=false`; these are engineering/generalization diagnostics, not publication-grade absolute-performance claims.

Generated graph identities from the retained run:

- `road-smallworld`: SHA-256 `4224bcf74f5246257aad13bd5e4a2222cdf423a011f8aba5c0da24987ccdb0e4`.
- `clustered-undirected`: SHA-256 `27290d6281eb2f7a83a87728bfb3edab073e405e2dcb32d0c742b46ae403d379`.

## H1 — frozen-selector held-out BFS

All policies were exact across all 18 files / 486 measured batches, with zero internal repair-to-full fallbacks in the adaptive policy.

Adaptive outcome:

| Metric | Observed hosted result |
| --- | ---: |
| Exactness | **100%** |
| Mean oracle regret | **9.019%** |
| Median oracle regret | **0%** |
| p95 oracle regret | **52.039%** |
| p99 oracle regret | **70.298%** |
| Maximum per-batch regret | **90.957%** |
| Wrong-arm rate | **5.144%** |
| Full-choice fraction | **3.292%** |
| Internal fallback fraction | **0%** |
| Mean decision cost | **0.209 µs** |

This is **mixed/negative generalization evidence**, not a passed development gate. Mean and especially tail regret are materially worse than the previously reported development suite. The result must not be suppressed or retrospectively tuned while still being described as held out.

Several large-batch road regimes show high wrong-arm rates despite bounded regret. This suggests that the current selector can generalize exactness and avoid hidden fallback while still misclassifying the cheaper execution arm on an unseen low-degree/high-diameter family.

## H2 — clean A0–A7 same-stream ablation

All eight stages were exact in all four retained runs. The clean ablation confirms two useful causal observations on this workload:

1. Moving from canonical-CSR rebuild (`A0`) to compact mutable storage (`A1`) removes a very large storage/rebuild penalty.
2. Selector-owned recomputation (`A7`) removes all internal repair-to-full fallback events observed in `A3`–`A6`.

However, `A7` does **not** dominate every earlier stage in oracle regret on this held-out workload. Aggregated mean oracle regret was approximately **18.0%** for `A7` versus **15.9%** for localized repair alone (`A2`). The scientifically safe interpretation is therefore that selector ownership removes hidden double work, while policy quality remains workload dependent; this hosted ablation does not establish monotonic improvement from every added mechanism.

## H3 — cross-algorithm exact triangle policy

All 84 measured batches were exact and there were zero internal fallbacks, but the adaptive triangle policy exposed a clear large-batch failure regime.

| Metric | Observed hosted result |
| --- | ---: |
| Exactness | **100%** |
| Mean oracle regret | **43.345%** |
| Median oracle regret | **0%** |
| p95 oracle regret | **29.430%** |
| p99 oracle regret | **939.689%** |
| Maximum per-batch regret | **1002.069%** |
| Wrong-arm rate | **4.762%** |
| Internal fallback fraction | **0%** |
| Mean decision cost | **0.243 µs** |

The aggregate mean is dominated by the preregistered `1024`-edge regime: all four batches selected the wrong arm, with mean regret about **870.5%**. The `64`- and `256`-edge regimes were much closer to the oracle.

This result supports transfer of the **measurement framework and exact two-arm execution contract**, but it does **not** support a claim that the current adaptive decision rule generalizes successfully across algorithms. The large-batch triangle failure is retained as a falsifying/stress result and should motivate a future algorithm-conditioned policy only in a new development campaign.

## Consequence for future experiments

No selector or threshold should be changed using these results and then evaluated again on these same workloads as if they were still unseen. If the BFS selector or triangle policy is redesigned after this observation, these workloads become development evidence and a **new preregistered holdout** is required for any fresh generalization claim.

Publication-grade conclusions still require the existing controlled dedicated-hardware campaign, same-machine external systems, hardware counters/NUMA where claimed, and independent reproduction.