# Ablation study

This document separates **measured evidence** from **proposed causal interpretation**. VeloGraphX has accumulated several direct A/B and selector-development experiments, but historical runs were not all produced by one cumulative seven-stage binary. They therefore must not be presented as if every row were an apples-to-apples component ablation.

## Research question

Which mechanisms are responsible for VeloGraphX's answer-ready latency and adaptive-policy quality?

The intended causal chain is:

`compact mutable storage → localized exact repair → affected-work fallback → scale conditioning → online cost prediction → uncertainty-aware decisions → selector-owned recomputation`

For every stage, the paper should report answer-ready batch latency, mean oracle-relative regret, p95 batch regret, worst-regime regret, selector overhead where applicable, and exactness.

## Validated evidence already available

| Mechanism/evidence | Controlled observation | Interpretation |
| --- | --- | --- |
| Storage maintenance optimization | `ca-GrQc`: 272.045 µs baseline → 115.771 µs optimized; `web-Google`: ~35.542 ms → ~35.303 ms; exact | Direct A/B evidence that storage/maintenance work materially matters on the smaller dynamic workload while remaining neutral on the large workload. |
| Localized exact repair | Dynamic BFS and triangle campaigns maintain independently verified exact answers; LiveJournal exact triangles show 3,228.66× / 382.85× / 33.99× incremental-vs-full speedups at 0.01% / 0.1% / 1% update sizes | Demonstrates the value of localized exact work when update impact is small; it does not by itself prove that incremental repair is always preferable. |
| Affected-work fallback | Early affected-only selector showed strong average cases but tail failures caused by discovering repair work before falling back | Motivates preflight/selector mechanisms that avoid paying repair discovery plus full recomputation. |
| Two-stage preflight + affected fallback | Development run: 3.783% mean regret, 33.43% worst-regime regret, exact | Mean improved substantially, but tail gate failed; update density alone was insufficient. |
| Root-state / scale-aware confidence model | Cold-start-guard run: 4.877% mean, 20.163% p95, 90.165% worst, exact | Root/scale state improved average behavior but exposed a large-graph tail corridor. |
| Confidence-aware large-graph guard | 5.545% mean, 18.461% p95, 36.560% worst, exact | Reduced the catastrophic tail while slightly worsening mean regret. |
| Scale-conditioned online predictor | 4.252% mean, 21.920% p95, 37.539% worst, exact | Conditioning prediction by graph scale restored mean performance but still missed tail gates. |
| Uncertainty-aware large-scale prediction | 4.050% mean, 25.049% p95, 29.630% worst, exact | Improved mean and worst-regime regret but uncertainty overlap still repeatedly selected an incremental path that could internally fall back. |
| Selector-owned large-scale recomputation | **3.148% mean, 19.780% p95, 18.253% worst, 100% exact** | Removing hidden repair→full double work on large adaptive runs was the final mechanism that passed all fixed development gates. |

## Final development result

The frozen `scale-conditioned-selector-owned-v3` selector was evaluated on the three checksum-pinned development graph families (`ca-GrQc`, `soc-Epinions1`, `web-Google`) under fixed acceptance criteria.

| Metric | Final result | Gate |
| --- | ---: | ---: |
| Exactness | **100%** | 100% |
| Mean regret | **3.148%** | ≤ 5% |
| p95 batch regret | **19.780%** | ≤ 20% |
| Worst-regime regret | **18.253%** | ≤ 25% |
| Mean decision cost | **7.486 µs** | reported, not tuned as an acceptance gate |
| Mean one-time selector setup | **72.827 µs** | reported separately |

Frozen benchmark source commit: `4381113005c221e6db2c19bac753a57d872e6374`  
Frozen benchmark blob: `c30f3459d22e61fb95abdb077aa2fd0eda445690`

## Clean policy-level ablation contract

The repository's `adaptive-policy-ablation.yml` workflow compares four execution policies under the same graph/update stream and exactness contract:

- `always_full`: full recomputation after every batch.
- `always_incremental`: localized repair without adaptive selector intervention.
- `simple_threshold`: fixed update-density decision rule.
- `adaptive`: the frozen workload-aware selector.

This is the cleanest current apples-to-apples ablation for the **value of adaptive selection as a whole**. It should remain distinct from the historical mechanism ledger above.

## Required component-level ablation for a paper

For a publication figure, implement each stage as an explicit compile-time or runtime feature switch in a dedicated ablation harness, leaving the production implementation and frozen selector unchanged. Run every stage on identical pinned datasets, roots, update streams, repetitions, compiler settings, and hardware.

| Stage | Enabled mechanism | Main hypothesis |
| --- | --- | --- |
| A0 | Full recomputation | Reference cost |
| A1 | Compact mutable storage + full recomputation | Isolate storage/update-path contribution |
| A2 | + localized exact repair | Quantify repair benefit |
| A3 | + affected-work fallback | Quantify protection from oversized repair |
| A4 | + graph-scale/root-state conditioning | Quantify structural context |
| A5 | + online cost prediction | Quantify learned execution-cost signal |
| A6 | + uncertainty-aware decision | Quantify protection from noisy predictions |
| A7 | + selector-owned recomputation | Quantify removal of hidden repair→full double work |

The result table must include both absolute answer-ready latency and oracle-relative policy metrics. Exactness verification must stay outside the timed region but execute after every batch.

## Statistical reporting

For each stage report at minimum:

- mean and median answer-ready batch latency;
- p95 and worst batch latency/regret;
- mean oracle-relative regret;
- number/fraction of full recomputations;
- mean selector decision cost and one-time setup cost;
- exactness rate;
- repetitions and confidence interval or bootstrap interval for aggregate comparisons.

Do not infer causality from historical runs that changed more than one mechanism. The historical ledger is useful motivation; the explicit A0–A7 harness is the publication-grade causal experiment.

## Held-out discipline

Held-out datasets must not be used to choose thresholds or mechanisms and then described as unseen. Development ablations should use development datasets only. Any redesigned selector requires a newly pre-registered unseen dataset for a fresh generalization claim.
