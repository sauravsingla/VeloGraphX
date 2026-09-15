# Manuscript results ledger

This file maps every planned manuscript figure/table to retained evidence and defines the exact claim boundary. It is the pre-submission guard against unsupported or cross-run comparisons.

## Figure 1 — system overview

**Purpose:** explain the execution path rather than establish performance.

**Content:** update batch → mutable graph substrate → exact repair candidate / full recomputation candidate → adaptive selector → exact result → bounded storage consolidation.

**Evidence dependency:** implementation and architecture only.

**Primary references:** `../docs/architecture.md`, `../docs/dynamic-storage.md`, `../benchmarks/adaptive_policy_bfs.cpp`.

**Claim boundary:** architecture figure must not attach universal speedup labels.

## Figure 2 — repair versus recomputation crossover and selector quality

**Purpose:** primary paper figure. Establish that the preferred exact execution mode changes by regime and evaluate the current publication selector against exact alternatives.

**Required series:** `always_incremental`, `always_full`, `simple_threshold` where available, current publication/adaptive policy, and exact oracle.

**Required reporting:** answer-ready latency or normalized latency by regime; exactness; oracle-relative regret; tail metric; selector decision overhead; internal fallback/wrong-arm diagnostic where supported.

**Current evidence status:** the submitted value must come from the audited current publication-selector run/artifact. Historical `scale-conditioned-selector-owned-v3` metrics in `../docs/ablation-study.md` are development evidence, not a silent substitute for the current `bounded-one-sided-warmup-v5` / publication-policy implementation.

**Fallback safe wording before audit:** “The exact strategies exhibit a crossover: localized repair wins in smaller-impact regimes while full recomputation becomes preferable as affected work grows.”

**Do not:** cite historical v3 regret as the current production selector without an explicit historical label.

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

**Purpose:** demonstrate exact maintained-analytics breadth beyond BFS with semantically fair answer-ready timing.

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

**Purpose:** show that mutable-storage maintenance can become dominated by repeated O(E) canonicalization and quantify the bounded memory/performance trade-off.

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

**Fresh run:** `34927599394`.

**Artifacts:**
- `facebook-combined`: `10380526227`, SHA-256 `6c31bd3032123d91c400715fe598ec820f93290a0b0673d9f94947a13014b3d6`
- `p2p-Gnutella08`: `10381010037`, SHA-256 `171727df8327519da4d45d4827df855c93b63131df12a1d5e001bc7e7dac54ae`
- `ca-HepTh`: `10379783827`, SHA-256 `f710ca8e08b6b90f25784a7868c2d33ee116608f6439da7e2b19fd33344175f9`

Five repetitions are retained at 13 update fractions for each dataset and every incremental result matches full recomputation.

**Role:** supporting evidence that the incremental/full crossover is graph-dependent for another exact analytic. This workflow is triangle-counting engineering evidence; it must not be presented as the central BFS adaptive-policy experiment.

**Observed crossover note:** within this hosted campaign, `facebook-combined` remains incremental-favorable through the largest tested ratio, while `p2p-Gnutella08` and `ca-HepTh` cross into full-recompute-favorable regimes at sufficiently large update ratios. If plotted, label ratios precisely rather than casually describing `1.5` or `2.0` as percentages.

## Supporting result — small hosted multicore scaling

README-recorded scoped values at 4 threads:
- BFS: 2.74×
- connected components: 2.50×
- triangles: 2.24×

**Role:** implementation maturity only.

**Do not:** extrapolate to 8/16/32 threads or NUMA.

## Supporting result — compression

README-recorded compression: 3.25×–3.78× smaller with a traversal-performance trade-off.

**Role:** optional storage subsection or appendix.

## Statistics and presentation rules

- Prefer median latency for repeated timings and include dispersion/error bars when raw samples are available.
- Report sample count/repetitions in captions.
- Report exactness in the same table/caption as dynamic performance.
- Use within-run ratios for cross-system comparison unless systems truly ran within the same timing envelope.
- Keep per-dataset results visible when winner reversals occur.
- Do not average VeloGraphX and competitor timings across unrelated GitHub runners.
- Do not replace a current selector result with a historical development metric merely because the historical number is stronger.

## Results intentionally excluded from the main paper until audited

1. A unified VeloGraphX / NetworKit / RisGraph league table from the three-system workflow.
2. Historical selector v3 headline regret as a current-selector result.
3. Dedicated many-core/NUMA/hardware-counter claims.
4. Out-of-core/NVMe peak-performance claims.
5. Any separate-run absolute timing comparison that violates the benchmark timing contract.
