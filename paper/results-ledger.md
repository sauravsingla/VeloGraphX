# Manuscript results ledger

This file maps manuscript-facing figures/tables to retained evidence and defines exact claim boundaries. It is the pre-submission guard against unsupported comparisons, post-result retuning, and selective omission of negative results.

## Figure 1 — system overview

**Purpose:** explain the execution path rather than establish performance.

**Content:** update batch → mutable graph substrate → pre-repair selector → localized exact repair or exact full recomputation → exact result; explicit bounded storage consolidation is a separate maintenance action.

**Primary references:** `../docs/architecture.md`, `../docs/dynamic-storage.md`, `../benchmarks/adaptive_policy_bfs.cpp`, `../benchmarks/publication_policy_bfs.cpp`.

**Claim boundary:** architecture figure must not attach universal speedup labels.

## Figure 2 — repair/recompute crossover and current selector quality

### Primary cross-dataset evidence

- **Run:** `34929398888`
- **Artifact:** `10381490811`
- **SHA-256:** `a78a421663b08228a7bd26596f9ad0498e2ad4a0c1470c79471dcaef97885a7b`
- **Policy:** `publication-preflight-v1`
- **Datasets / roots:** `ca-GrQc:1974`, `soc-Epinions1:71391`, `web-Google:391806`
- **Regimes:** three batch sizes per dataset, nine regimes total
- **Repetitions:** five per regime
- **Adaptive batch observations:** 1,610 sequential observations
- **Threads:** one
- **Exactness:** 100%; policy outputs checked against exact BFS

| Metric | Audited value |
| --- | ---: |
| Equal-regime mean oracle regret | 3.939% |
| Sample-weighted mean oracle regret | 2.309% |
| Sample-weighted wrong-arm rate | 1.739% |
| Sample-weighted selector decision cost | 0.286 µs |
| Worst-regime mean regret | 17.477% |
| Worst-regime p95 regret | 54.424% |
| Maximum single-batch regret | 86.619% |

The largest evaluated `web-Google` regime remains visible: batch 24,576 has 17.477% mean regret, 54.424% p95 regret, and 33.3% wrong-arm rate across 15 batch samples. All three `soc-Epinions1` regimes have zero wrong-arm selections and roughly 0.86%–1.10% mean regret.

**Safe statement:** exact repair and recomputation exhibit a regime-dependent crossover; the frozen current selector has low average regret on this historical fixed graph/root program while retaining a material tail.

**Do not:** describe this program as a newly unseen holdout, claim uniform near-oracle performance, or replace the current result with stronger historical development metrics.

### Same-graph regression evidence

- **Run:** `34928935983`
- **Artifact:** `10381480310`
- **SHA-256:** `800d53a327900b4a579bb761d07adf86e3622b4e46a1a8ddea0fb25603bf3691`
- **Dataset/root:** checksum-pinned `web-Google:481807`
- **Batch sizes:** 512, 2,048, 8,192, 32,768
- **Repetitions:** three per regime
- **Exactness:** 100%
- **Mean regret:** 2.466% across regimes
- **Worst-regime p95:** 15.671%
- **Maximum single-batch regret:** 27.717%

Use only as focused regression evidence, not as a substitute for cross-dataset validation.

## Submission-closure Table/Figure A — production 0.35 fallback and avoided double work

- **Run:** `35237513376`
- **Artifact:** `10503926632`
- **SHA-256:** `26c37720fef6524ee68e816a095f7dd591c5cfd93c2f112a8de7ebd8874a621a`
- **Head SHA used by retained run:** `be8ca8be81d18028db0c38fdbbd87c260d9cb73c`
- **Exactness:** 93/93 aligned observations exact

| Metric | Retained result |
| --- | ---: |
| Fallback-only internal fallbacks | 6 |
| Fallback opportunities avoided by frozen pre-repair selector | 6 |
| Remaining selector-path internal fallbacks | 0 |
| Explicit-full/non-fallback labels (historical `false_full` field) | 33 |
| Conservatively observed fallback double work | 17.323 ms |
| Conservatively observed double work avoided | 17.323 ms |

**Interpretation:** this campaign closes the gap intentionally left by the clean-oracle-arm primary selector harness. Under the production 0.35 affected-region fallback, pre-repair selection avoided all six observed fallback opportunities and therefore avoided the conservatively measured 17.323 ms of repair-discovery-then-full work. All six fallback events occur in the declared 220K destructive-cascade stress case; the 72 real-graph observations have zero 0.35 fallbacks. Across all 93 observations, selector+fallback reduces cumulative answer-ready latency by 42.7% relative to fallback-only (1.745× ratio); on real graphs alone the reduction is 42.2% (1.729×).

**Historical field semantics:** the 33 `false_full` labels mean explicit full was selected on a batch where fallback-only did not internally fall back. They are not a wrong-arm count; the retained positive false-full penalty is 0 µs.

**Required caveat:** the 220K destructive cascade demonstrates the fallback mechanism, not its natural-workload frequency. Cumulative latency totals are campaign-scoped and sample-weighted.

## Submission-closure Table/Figure B — frozen held-out generalization

- **Run:** `35237513595`
- **Artifact:** `10504131673`
- **SHA-256:** `3cc4394693818450523fc56a9bb4368f7e26365d19077f2835a915a2bc312452`
- **Selector:** frozen before data acquisition
- **Post-result retuning:** none
- **Exactness:** 100%

| Held-out scope | Regimes | Equal-regime mean regret | Worst-regime mean | Worst-regime p95 |
| --- | ---: | ---: | ---: | ---: |
| All held-out regimes | 15 | 21.32% | 185.83% | 311.88% |
| Timestamp-ordered `CollegeMsg` | 9 | 34.52% | 185.83% | 311.88% |
| Unseen `Amazon0312` | 6 | 1.52% | 2.29% | 9.44% |

`CollegeMsg` preserves timestamp arrival order. Repeated interactions are idempotent under simple-graph semantics and window removals are induced expiries rather than observed deletions.

**Safe statement:** the frozen selector generalizes well to the retained unseen Amazon regimes but poorly to the retained temporal CollegeMsg regimes; exactness is unaffected because plan errors are performance errors only.

**Required negative-result statement:** the current hand-designed selector does **not** demonstrate uniformly low-regret out-of-sample generalization. The architecture remains the contribution; policy learning/generalization remains an open systems problem.

**Do not:** average away the CollegeMsg tail, retune thresholds after this result, or describe induced window expiries as observed deletion events.

## Submission-closure Table/Figure C — one-mechanism-at-a-time selector ablation

- **Run:** `35237513377`
- **Artifact:** `10504591544`
- **SHA-256:** `a0aa985cd3f64d2e340938ba0111ec017076e24412665213bd4a9f4d5bf3601d`
- **Exactness:** all variants exact

| Policy | Equal-regime mean regret | Worst-regime mean | Worst-regime p95 |
| --- | ---: | ---: | ---: |
| `adaptive` | 4.82% | 18.76% | 58.39% |
| `adaptive_no_structural` | 8.16% | 42.16% | 131.88% |
| `adaptive_no_affected` | 4.20% | 17.53% | 56.07% |
| `adaptive_no_uncertainty` | 142.97% | 1160.11% | 3558.37% |
| `simple_threshold` | 17.24% | 59.65% | 141.10% |
| `always_incremental` | 11.45% | 59.03% | 131.93% |
| `always_full` | 632.00% | 1756.90% | 4451.34% |

Each `adaptive_no_*` variant removes exactly one mechanism under the same graph/root/batch/repetition/timing contract.

**Supported mechanism conclusions:** structural preflight improves both average and tail regret; the uncertainty guard is essential in this harness and prevents catastrophic overconfident full choices.

**Negative/neutral mechanism conclusion:** removing the previous-affected-work factor slightly improves aggregate metrics in this campaign. Do not claim that this feature is independently beneficial based on this ablation.

## External-baseline table

Preserve run boundaries; do not combine absolute times from unrelated campaigns.

| Comparison | Retained run | Artifact | Scoped result | Required caveat |
| --- | ---: | ---: | --- | --- |
| VeloGraphX vs NetworKit `DynBFS` | `33542995289` | `9814639042` | `web-Google`: VeloGraphX ≈1.38× lower latency; `ca-GrQc`: NetworKit ≈1.35× lower latency; 30/30 paired executions exact | One thread, two graph families |
| VeloGraphX vs RisGraph | `33286241439` | `9724535579` | RisGraph remains ≈1.90× faster on the documented hosted `web-Google` run; VeloGraphX repair beats its own legacy full path | Separate run from NetworKit |
| VeloGraphX vs GAP + LAGraph, BFS | `33418520303` | `9768499895` | VeloGraphX wins tested hosted 1/2/4-thread BFS cases: 1.60×–2.04× vs GAP, 9.4×–11.8× vs LAGraph | Not a many-core claim |
| VeloGraphX vs GAP + LAGraph, weighted SSSP | `33418520303` | `9768499895` | GAP wins; VeloGraphX 2.6×–3.0× faster than LAGraph but 7.0×–8.5× slower than GAP | Retain competitor win |
| VeloGraphX vs pinned GraphBolt real-dataset BFS | `35237513587` | `10503851688` | GraphBolt/VeloGraphX answer-ready ratio = 14.219× at 0.1%, 2.245× at 1%, 0.886× at 5%; five paired reps each; outputs verified | Same hosted allocation/shared mutation stream; pinned legacy GraphBolt runtime; retain winner reversal |

For the GraphBolt ratios, values above 1 mean VeloGraphX lower latency; the 0.886× value means GraphBolt is faster at the largest tested fraction.

## Exact dynamic triangles against published exact reference

- **Run:** `33248107299`
- **Artifact:** `9713495404`
- **Dataset:** normalized `facebook-combined`, 4,039 vertices, 88,234 undirected edges
- **Repetitions:** five per fraction, 15/15 exact

| Insert batch | VeloGraphX answer-ready | GoldenCounter answer-ready | GoldenCounter / VeloGraphX |
| --- | ---: | ---: | ---: |
| 1% / 883 edges | 1.066 ms | 43.657 ms | 40.95× |
| 5% / 4,412 edges | 6.782 ms | 47.095 ms | 6.94× |
| 10% / 8,824 edges | 15.495 ms | 53.931 ms | 3.48× |

**Safe statement:** same exact post-update triangle count with the listed lower median answer-ready latencies against the pinned exact GoldenCounter reference.

**Do not:** claim superiority over the paper's approximate SWTC algorithm; semantics differ.

## Large-graph canonicalization A/B

- **Run:** `33265264254`
- **Artifact:** `9718634869`
- **Dataset:** SNAP `com-Orkut`, 3,072,441 vertices, 234,370,166 directed arcs, 60 epochs

| Metric | 1.25× envelope | 1.50× envelope | Change |
| --- | ---: | ---: | ---: |
| Consolidations | 15 | 6 | −60% |
| Total consolidation time | 386.573 s | 156.128 s | −59.6% |
| Maintenance-amortized throughput | 19,135 ops/s | 43,062 ops/s | 2.25× |
| Peak RSS | 7,517,892 KiB | 8,016,740 KiB | +6.6% |

**Safe statement:** the wider bounded envelope materially reduces canonicalization cost on this retained workload at a measurable memory price.

**Do not:** claim 1.50× is universally optimal.

## Supporting multi-dataset triangle crossover

**Run:** `34927599394`.

- `facebook-combined`: artifact `10380526227`, SHA-256 `6c31bd3032123d91c400715fe598ec820f93290a0b0673d9f94947a13014b3d6`
- `p2p-Gnutella08`: artifact `10381010037`, SHA-256 `171727df8327519da4d45d4827df855c93b63131df12a1d5e001bc7e7dac54ae`
- `ca-HepTh`: artifact `10379783827`, SHA-256 `f710ca8e08b6b90f25784a7868c2d33ee116608f6439da7e2b19fd33344175f9`

Five repetitions are retained at 13 update fractions per dataset and every incremental result matches full recomputation. Use as supporting evidence that crossover is graph-dependent outside BFS, not as the central selector result.

## Statistical and presentation rules

- Prefer medians for repeated latency and include dispersion when raw samples permit it.
- Report sample count/repetitions and exactness beside dynamic performance.
- Use within-run ratios for cross-system comparison unless systems truly share one timing envelope.
- Keep winner reversals, false-full choices, tail failures, and held-out failures visible.
- Do not average absolute timings across unrelated GitHub runners.
- Do not replace a current selector result with a stronger historical development metric.
- Do not retune `publication-preflight-v1` against the held-out CollegeMsg result and then call the same data a holdout.
- PageRank must be described with residual/tolerance semantics, not as mathematically exact.

## Submission freeze

The immutable submission tag is `pvldb-2027-submission-v4`, as recorded in `submission-freeze.json`. The exact v4 software/reproducibility artifact is published on Zenodo at DOI `10.5281/zenodo.22842292`. This DOI identifies the archived submission artifact; it is not a PVLDB publication DOI and does not imply venue acceptance.

## Results still excluded from headline claims

1. A synthetic combined VeloGraphX / NetworKit / RisGraph league table that merges separate-run absolute times.
2. Dedicated many-core/NUMA/hardware-counter claims until suitable controlled hardware executes them.
3. Out-of-core/NVMe peak-performance claims without dedicated evidence.
4. Any claim that the present selector is uniformly general across unseen temporal workloads.
