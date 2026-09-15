# Manuscript results ledger

This file maps every planned manuscript figure/table to retained evidence and defines the exact claim boundary. It is the pre-submission guard against unsupported or cross-run comparisons.

## Figure 1 — system overview

**Purpose:** explain the execution path rather than establish performance.

**Content:** update batch → mutable graph substrate → exact repair candidate / full recomputation candidate → pre-repair selector → exact result → bounded storage consolidation.

**Primary references:** `../docs/architecture.md`, `../docs/dynamic-storage.md`, `../benchmarks/adaptive_policy_bfs.cpp`, `../benchmarks/publication_policy_bfs.cpp`.

**Claim boundary:** architecture figure must not attach universal speedup labels.

## Figure 2 — repair versus recomputation crossover and current selector quality

**Purpose:** primary paper figure. Establish that the preferred exact execution mode changes by regime and evaluate the current `publication-preflight-v1` selector against exact alternatives.

### Primary cross-dataset evidence

- **Run:** `34929398888`
- **Artifact:** `10381490811`
- **SHA-256:** `a78a421663b08228a7bd26596f9ad0498e2ad4a0c1470c79471dcaef97885a7b`
- **Policy source:** current `benchmarks/publication_policy_bfs.cpp`, layered over historical selector source `bounded-one-sided-warmup-v5`
- **Datasets / fixed roots:** `ca-GrQc:1974`, `soc-Epinions1:71391`, `web-Google:391806`
- **Regimes:** three batch sizes per dataset, nine regimes total
- **Repetitions:** five per regime
- **Adaptive batch samples:** 1,610
- **Threads:** one
- **Exactness:** 100%; all policy outputs checked against exact BFS

Current selector summary:

| Metric | Audited value |
| --- | ---: |
| Mean oracle regret across nine regimes | 3.939% |
| Sample-weighted mean oracle regret | 2.309% |
| Sample-weighted wrong-arm rate | 1.739% |
| Sample-weighted selector decision cost | 0.286 µs |
| Internal full fallbacks | 0 |
| Redundant historical one-sided-full decisions | 0 |
| Worst-regime mean regret | 17.477% |
| Worst-regime p95 regret | 54.424% |
| Maximum single-batch regret | 86.619% |

The tail limitation must remain visible. It is concentrated in the largest evaluated `web-Google` regime: batch size 24,576 has 17.477% mean regret, 54.424% p95 regret, and a 33.3% wrong-arm rate across 15 batch samples. By contrast, all three `soc-Epinions1` regimes have zero wrong-arm selections and roughly 0.86%–1.10% mean regret.

Per-regime adaptive values are retained in [`data/current-selector-regimes.csv`](data/current-selector-regimes.csv).

### Same-graph tail-validation evidence

- **Run:** `34928935983`
- **Artifact:** `10381480310`
- **SHA-256:** `800d53a327900b4a579bb761d07adf86e3622b4e46a1a8ddea0fb25603bf3691`
- **Dataset:** checksum-pinned `web-Google`, root `481807`
- **Batch sizes:** 512, 2,048, 8,192, 32,768
- **Repetitions:** three per regime
- **Exactness:** 100%
- **Mean regret across regimes:** 2.466%
- **Worst-regime p95 regret:** 15.671%
- **Maximum single-batch regret:** 27.717%
- **Internal fallbacks / redundant one-sided-full decisions:** 0 / 0
- **Sample-weighted selector decision cost:** about 1.10 µs

Use this as a focused tail-regression check, not as a substitute for the three-graph result.

### Figure design

Show `always_incremental`, `always_full`, `simple_threshold`, `history_cost_model`, current adaptive policy, and the exact per-batch oracle where the figure remains readable. Prefer normalized answer-ready latency or oracle-relative regret by regime. Exactness and sample count must be visible in the caption.

**Safe statement:** the exact strategies exhibit a crossover; the current pre-repair selector remains exact and has low average regret across the evaluated graph families, while retaining a visible tail limitation on the largest `web-Google` regime.

**Do not:** use historical v3 regret as the current selector result; claim that the current selector is uniformly near-oracle; or describe this historical fixed graph/root program as a newly unseen holdout.

## Historical selector development — not a headline result

`../docs/ablation-study.md` records the frozen `scale-conditioned-selector-owned-v3` development campaign: 100% exactness, 3.148% mean regret, 19.780% p95 batch regret, and 18.253% worst-regime regret. Those values explain design evolution only. The current manuscript uses the audited `publication-preflight-v1` evidence above.

## Table 1 — external baseline summary

Use separate rows and preserve timing/run boundaries.

| Comparison | Retained run | Artifact | Safe manuscript statement | Required caveat |
| --- | ---: | ---: | --- | --- |
| VeloGraphX vs NetworKit `DynBFS` | `33542995289` | `9814639042` | `web-Google`: VeloGraphX ≈1.38× faster; `ca-GrQc`: NetworKit ≈1.35× faster; 30/30 paired executions exact | 1 thread, 2 graph families; paired-run conclusion only |
| VeloGraphX vs GAP + LAGraph, BFS | `33418520303` | `9768499895` | VeloGraphX fastest in tested 1/2/4-thread BFS cases; 1.60×–2.04× vs GAP and 9.4×–11.8× vs LAGraph | Hosted 1–4-thread result, not many-core claim |
| VeloGraphX vs GAP + LAGraph, weighted SSSP | `33418520303` | `9768499895` | GAP fastest; VeloGraphX 2.6×–3.0× faster than LAGraph but 7.0×–8.5× slower than GAP | Retain competitor win |
| VeloGraphX vs RisGraph | `33286241439` | `9724535579` | Localized VeloGraphX repair improves materially over legacy full-recompute policy; RisGraph remains faster on the evaluated `web-Google` workload | Separate run from NetworKit; never combine absolute times into a three-system ranking |

**Presentation recommendation:** report dimensionless within-run ratios and exactness rather than juxtaposing absolute microseconds from unrelated runs.

## Figure 3 — exact dynamic triangles against published exact reference

**Run:** `33248107299`  
**Artifact:** `9713495404`  
**Dataset:** normalized `facebook-combined`, 4,039 vertices, 88,234 undirected edges.  
**Repetitions:** five per fraction, 15/15 exact.

| Insert batch | VeloGraphX answer-ready | GoldenCounter answer-ready | GoldenCounter / VeloGraphX |
| --- | ---: | ---: | ---: |
| 1% / 883 edges | 1.066 ms | 43.657 ms | 40.95× |
| 5% / 4,412 edges | 6.782 ms | 47.095 ms | 6.94× |
| 10% / 8,824 edges | 15.495 ms | 53.931 ms | 3.48× |

**Safe statement:** VeloGraphX reaches the same exact post-update count with the listed lower median answer-ready latencies against the pinned exact GoldenCounter reference implementation.

**Do not:** claim superiority over the SIGMOD 2021 approximate SWTC algorithm; semantics differ.

## Figure 4 — large-graph canonicalization A/B

**Run:** `33265264254`  
**Artifact:** `9718634869`  
**Dataset:** SNAP `com-Orkut`, 3,072,441 vertices, 234,370,166 directed arcs, 60 epochs.

| Metric | 1.25× envelope | 1.50× large-graph envelope | Change |
| --- | ---: | ---: | ---: |
| Consolidations | 15 | 6 | −60% |
| Total consolidation time | 386.573 s | 156.128 s | −59.6% |
| Maintenance-amortized throughput | 19,135 ops/s | 43,062 ops/s | 2.25× |
| Peak RSS | 7,517,892 KiB | 8,016,740 KiB | +6.6% |

**Safe statement:** on this retained Orkut workload, a wider but still bounded envelope materially reduces full canonicalization cost at a measurable memory price.

**Do not:** claim the 1.50× threshold is universally optimal.

## Supporting figure/table — multi-dataset triangle crossover

**Run:** `34927599394`.

Artifacts:

- `facebook-combined`: `10380526227`, SHA-256 `6c31bd3032123d91c400715fe598ec820f93290a0b0673d9f94947a13014b3d6`
- `p2p-Gnutella08`: `10381010037`, SHA-256 `171727df8327519da4d45d4827df855c93b63131df12a1d5e001bc7e7dac54ae`
- `ca-HepTh`: `10379783827`, SHA-256 `f710ca8e08b6b90f25784a7868c2d33ee116608f6439da7e2b19fd33344175f9`

Five repetitions are retained at 13 update fractions per dataset and every incremental result matches full recomputation. Use only as supporting evidence that crossover is graph-dependent for another exact analytic; it is not the central BFS policy experiment.

## Supporting maturity evidence

Hosted 4-thread speedups recorded by the project are 2.74× for BFS, 2.50× for connected components, and 2.24× for triangles. Compression is approximately 3.25×–3.78× smaller with a traversal trade-off. These are supporting implementation results only; do not extrapolate them to many-core or NUMA behavior.

## Statistics and presentation rules

- Prefer median latency and include dispersion/error bars when raw samples are available.
- Report sample count/repetitions in captions.
- Report exactness beside dynamic performance.
- Use within-run ratios for cross-system comparison unless systems truly ran within the same timing envelope.
- Keep per-dataset reversals and selector tail failures visible.
- Do not average absolute timings across unrelated GitHub runners.
- Do not replace a current selector result with a stronger historical development metric.

## Results intentionally excluded from the main paper until audited

1. A unified VeloGraphX / NetworKit / RisGraph league table from the three-system workflow.
2. Dedicated many-core/NUMA/hardware-counter claims.
3. Out-of-core/NVMe peak-performance claims.
4. Any separate-run absolute timing comparison that violates the benchmark timing contract.
