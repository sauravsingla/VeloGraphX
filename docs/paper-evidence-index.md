# Paper Evidence Index

This document is the manuscript-facing index for VeloGraphX experimental evidence. It does **not** change historical `publication_grade` or `research_claim` flags stored by existing workflows. Instead, it states which retained hosted results are suitable for scoped manuscript claims and which claims remain outside the evidence envelope.

## Evidence-use policy

VeloGraphX distinguishes evidence by what a result can support, rather than by whether the machine is physically owned by the authors.

### Tier A — manuscript-ready hosted comparative evidence

A hosted result may support a paper claim when compared systems or policies execute under the same documented timing envelope on the same runner, revisions and datasets are pinned, repetitions are retained, and correctness gates pass. Tier A evidence is suitable for **scoped relative claims** such as crossover behavior, policy quality, paired system comparisons, exactness, and 1–4-thread behavior on the evaluated hosted runner.

Tier A does **not** establish universal peak throughput, many-core scaling, multi-socket NUMA behavior, hardware-counter superiority, or storage-device-specific performance.

### Tier B — manuscript-ready supporting hosted evidence

A controlled hosted A/B or published-reference comparison can support a narrowly stated mechanism or algorithm claim when provenance, exactness, workload, timing envelope, and repetitions are explicit. These results should be presented as supporting evidence rather than as universal system rankings.

### Tier C — development or pending-audit evidence

A result remains Tier C when the implementation/contract is useful but the final retained artifact has not been cross-referenced or audited for manuscript use. Tier C numbers must not appear as headline manuscript evidence until that audit is completed.

## Accepted hosted campaigns

| Evidence role | Tier | GitHub Actions run | Artifact / digest | Repetition and correctness contract | Safe manuscript use | Boundary |
| --- | --- | ---: | --- | --- | --- | --- |
| Current publication selector, cross-dataset | A | `34929398888` | `10381490811`; SHA-256 `a78a421663b08228a7bd26596f9ad0498e2ad4a0c1470c79471dcaef97885a7b` | Three pinned graph families × three regimes × five repetitions; 1,610 adaptive batch samples; all policies exact | Report exact current-policy crossover quality, 3.939% mean regime-level oracle regret, 2.309% sample-weighted regret, 1.739% wrong-arm rate, zero internal fallbacks, and scoped per-regime results | Historical fixed graph/root program, not a newly unseen holdout; retain the large-`web-Google` tail limitation |
| Current publication selector, focused tail regression | A/B | `34928935983` | `10381480310`; SHA-256 `800d53a327900b4a579bb761d07adf86e3622b4e46a1a8ddea0fb25603bf3691` | Pinned `web-Google`; four regimes; three repetitions per regime; all policies exact | Supporting evidence that the current policy removed redundant one-sided-full behavior with 2.466% mean regime regret and zero internal fallbacks | Same graph only; use as focused regression evidence, not a generalization claim |
| Native BFS/SSSP vs GAP and LAGraph | A | `33418520303` | `9768499895`; SHA-256 `a858749145ae2490f524672344358513daa68151471f83249601b2af62c7c6a7` | 1/2/4 threads; five repetitions per configuration; same graph/source/thread policy; independent correctness checks | Report same-run hosted relative BFS/SSSP behavior, including competitor wins | Do not generalize 4-vCPU hosted behavior to many-core or universal peak throughput |
| Dynamic BFS vs NetworKit `DynBFS` | A | `33542995289` | `9814639042`; SHA-256 `acf743bcac2542660fa050d70105e5e1e5f79d9e22ef1ec7324cf1e147ae5f12` | Three fixed roots × five paired repetitions × two datasets = 30 paired executions; every execution exact | Report that VeloGraphX is faster on the tested `web-Google` workload while NetworKit is faster on the tested `ca-GrQc` workload | One thread and two graph families; do not claim universal superiority |
| Dynamic BFS vs RisGraph | A/B | `33286241439` | `9724535579`; SHA-256 `ce0afc7e60dd0b0dc19f506324190745081215f4b7764f85b1ed2d3284d5d538` | `web-Google`; fixed root; 13 update batches; exact final BFS and independent verification | Report the same-run result that localized VeloGraphX repair improves over its legacy full-recompute policy while RisGraph remains faster on this workload | This run is separate from the NetworKit run; absolute times from the two runs must not be combined into a three-system ranking |
| Exact dynamic triangles vs published GoldenCounter reference | B | `33248107299` | `9713495404`; SHA-256 `f032beb3abb751f10deae0fe9a9db42ce823444990c9603ee354a59d0c1a6eb5` | 1%, 5%, 10% update batches; five paired repetitions per configuration; 15/15 exact | Report measured answer-ready latency ratios against the published exact GoldenCounter reference | Do not claim reproduction of the full DZiG/SWTC architecture or universal triangle superiority |
| 100M+-class canonicalization policy A/B | B | `33265264254` | `9718634869`; SHA-256 `f3d084ca2a0c28aaa84c3124498da63266169608371cd633f5390337c94dde74` | `com-Orkut`, 234,370,166 directed arcs; 60 epochs; exactness gates every epoch | Support the storage-maintenance claim: higher maintenance-amortized throughput and less consolidation time at a documented RSS trade-off | One retained hosted execution; do not generalize to all hardware or graph families |

## Current publication-selector evidence

The manuscript-selected policy is `publication-preflight-v1`, implemented in [`../benchmarks/publication_policy_bfs.cpp`](../benchmarks/publication_policy_bfs.cpp). It layers publication telemetry and a pre-repair selector over the frozen historical selector helpers rather than silently rewriting the old development record.

The primary cross-dataset run (`34929398888`) uses checksum-pinned `ca-GrQc`, `soc-Epinions1`, and `web-Google`, one thread, fixed roots, three regimes per graph, and five repetitions per regime. Across 1,610 adaptive batch samples:

- every result is exact;
- mean oracle regret across the nine regimes is **3.939%**;
- sample-weighted mean oracle regret is **2.309%**;
- sample-weighted wrong-arm rate is **1.739%**;
- sample-weighted selector decision cost is about **0.286 µs**;
- internal full fallbacks are **zero**; and
- the obsolete `large_one_sided_full` decision appears **zero** times.

The tail is not hidden. The largest evaluated `web-Google` regime (batch size 24,576) has 17.477% mean regret, 54.424% p95 regret, and a 33.3% wrong-arm rate over 15 batch samples. The paper should show or state this limitation instead of quoting only the aggregate mean.

A focused `web-Google` tail-regression run (`34928935983`) independently retains 100% exactness, 2.466% mean regret across four regimes, 15.671% worst-regime p95 regret, 27.717% maximum single-batch regret, zero internal fallbacks, and zero redundant one-sided-full decisions.

The validated cross-dataset contract is retained as [`.github/workflows/publication-selector-cross-dataset.yml`](../.github/workflows/publication-selector-cross-dataset.yml), and manuscript-ready per-regime values are retained in [`../paper/data/current-selector-regimes.csv`](../paper/data/current-selector-regimes.csv).

## Historical selector development

The frozen selector development record in [`ablation-study.md`](ablation-study.md) reports 100% exactness, 3.148% mean oracle-relative regret, 19.780% p95 batch regret, and 18.253% worst-regime regret for the historical `scale-conditioned-selector-owned-v3` campaign. The frozen benchmark source commit is `4381113005c221e6db2c19bac753a57d872e6374` and the recorded source blob is `c30f3459d22e61fb95abdb077aa2fd0eda445690`.

**Status: Tier C as a current-selector headline result.** Those numbers may describe historical design development, but the current manuscript uses the audited `publication-preflight-v1` artifacts above.

## Pending unified three-system campaign

The same-machine VeloGraphX / NetworKit / RisGraph campaign remains **Tier C** until its numerical matrix is independently audited and cross-referenced in this registry. Its workflow covers web, directed social, road, controlled R-MAT, and larger-social graphs across seven update fractions from 0.0001% through 10%.

Until that audit is complete:

1. use the accepted NetworKit and RisGraph campaigns independently;
2. never merge their absolute times into a single league table; and
3. present only within-run paired ratios or explicitly labelled workload-specific comparisons.

## Manuscript result selection

### Primary figure — repair/recompute crossover

Use the current selector evidence above. Show where localized exact repair is preferable, where full recomputation is preferable, and how the current policy tracks the per-batch oracle. Report exactness, average regret, tail regret, wrong-arm behavior, and selector cost. Do not hide the largest-`web-Google` tail.

### Primary external-baseline table

Keep winners and losses visible:

- NetworKit wins on `ca-GrQc` while VeloGraphX wins on the evaluated `web-Google` campaign;
- GAP wins weighted SSSP in the hosted static campaign while VeloGraphX wins the hosted BFS cases; and
- RisGraph wins the documented `web-Google` dynamic-BFS run even though VeloGraphX repair materially improves over its legacy full-recompute path.

### Supporting mechanism evidence

Use the GoldenCounter triangle comparison and Orkut canonicalization A/B as secondary evidence that the design extends beyond a single BFS result. The fresh multi-dataset triangle crossover (`34927599394`) can support graph-dependent crossover, but it must not displace the central BFS policy experiment.

## Statistical reporting policy

For manuscript tables and figures:

1. prefer median plus dispersion when raw repetitions are available;
2. retain raw per-repetition data and report repetition counts;
3. use paired comparisons when systems execute under the same runner/timing envelope;
4. keep exactness verification outside timed regions but require it before accepting a timing sample;
5. preserve negative results and tail failures rather than filtering to favorable regimes; and
6. label mean, median, p95, and sample-weighted metrics exactly rather than interchanging them.

## Claims intentionally excluded from hosted evidence

The current paper does not need, and hosted runners should not be used to establish:

- 8/16/32+ core scalability;
- true multi-socket NUMA locality or remote-memory superiority;
- stable hardware-counter improvements;
- AVX-512 or microarchitecture-specific universal speedups;
- research-scale NVMe / `io_uring` peak throughput; or
- universal system-wide fastest-performance claims.

These are optional future evaluations. They are **not blockers** for a paper centered on exact adaptive execution, crossover, same-run relative comparisons, and reproducibility.

## Artifact-retention note

GitHub Actions artifacts are retention-limited. Run IDs, immutable revisions, hashes, result documentation, and reproducible workflows preserve provenance, but a submission-era artifact package should freeze the selected raw results outside ephemeral Actions retention, for example in a tagged release and DOI-capable archive.

## Source documents

- [Paper manuscript workspace](../paper/README.md)
- [Manuscript results ledger](../paper/results-ledger.md)
- [Related-work / novelty notes](../paper/related-work-notes.md)
- [Hosted native competitor evidence](hosted-native-competitors.md)
- [External dynamic baselines](external-dynamic-baselines.md)
- [Published exact triangle baseline](same-run-published-baseline.md)
- [Canonicalization A/B evidence](canonicalization-ab-evidence.md)
- [Ablation study](ablation-study.md)
- [Benchmark methodology](benchmark-methodology.md)
- [Paper claim-to-evidence map](paper-claims.md)
