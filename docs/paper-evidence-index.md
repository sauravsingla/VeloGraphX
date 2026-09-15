# Paper Evidence Index

This document is the manuscript-facing index for VeloGraphX experimental evidence. It does **not** change historical `publication_grade` or `research_claim` flags stored by existing workflows. Instead, it states which retained hosted results are suitable for scoped manuscript claims and which claims remain outside the evidence envelope.

## Evidence-use policy

VeloGraphX distinguishes evidence by what a result can support, rather than by whether the machine is physically owned by the authors.

### Tier A — manuscript-ready hosted comparative evidence

A hosted result may support a paper claim when the compared systems or policies execute under the same documented timing envelope on the same runner, revisions and datasets are pinned, repetitions are retained, and correctness gates pass. Tier A evidence is suitable for **scoped relative claims** such as crossover behavior, paired system comparisons, exactness, and 1–4-thread behavior on the evaluated hosted runner.

Tier A does **not** establish universal peak throughput, many-core scaling, multi-socket NUMA behavior, hardware-counter superiority, or storage-device-specific performance.

### Tier B — manuscript-ready supporting hosted evidence

A controlled hosted A/B or published-reference comparison can support a narrowly stated mechanism or algorithm claim when provenance, exactness, workload, timing envelope, and repetitions are explicit. These results should be presented as supporting evidence rather than as universal system rankings.

### Tier C — development or pending-audit evidence

A result remains Tier C when the implementation/contract is useful but the final retained artifact has not yet been cross-referenced or audited for manuscript use. Tier C numbers must not appear as headline manuscript evidence until that audit is completed.

## Accepted hosted campaigns

| Evidence role | Tier | GitHub Actions run | Artifact / digest | Repetition and correctness contract | Safe manuscript use | Boundary |
| --- | --- | ---: | --- | --- | --- | --- |
| Native BFS/SSSP vs GAP and LAGraph | A | `33418520303` | `9768499895`; SHA-256 `a858749145ae2490f524672344358513daa68151471f83249601b2af62c7c6a7` | 1/2/4 threads; five repetitions per configuration; same graph/source/thread policy; independent correctness checks | Report same-run hosted relative BFS/SSSP behavior, including competitor wins | Do not generalize 4-vCPU hosted behavior to many-core or universal peak throughput |
| Dynamic BFS vs NetworKit `DynBFS` | A | `33542995289` | `9814639042`; SHA-256 `acf743bcac2542660fa050d70105e5e1e5f79d9e22ef1ec7324cf1e147ae5f12` | Three fixed roots × five paired repetitions × two datasets = 30 paired executions; every execution exact | Report that VeloGraphX is faster on the tested `web-Google` workload while NetworKit is faster on the tested `ca-GrQc` workload | One thread and two graph families; do not claim universal superiority |
| Dynamic BFS vs RisGraph | A/B | `33286241439` | `9724535579`; SHA-256 `ce0afc7e60dd0b0dc19f506324190745081215f4b7764f85b1ed2d3284d5d538` | `web-Google`; fixed root; 13 update batches; exact final BFS and independent verification | Report the same-run result that localized VeloGraphX repair improves over its legacy full-recompute policy while RisGraph remains faster on this workload | This run is separate from the NetworKit run; absolute times from the two runs must not be combined into a three-system ranking |
| Exact dynamic triangles vs published GoldenCounter reference | B | `33248107299` | `9713495404`; SHA-256 `f032beb3abb751f10deae0fe9a9db42ce823444990c9603ee354a59d0c1a6eb5` | 1%, 5%, 10% update batches; five paired repetitions per configuration; 15/15 exact | Report the measured answer-ready latency ratios against the published exact GoldenCounter reference for the evaluated workload | Do not claim reproduction of the full DZiG/SWTC architecture or universal triangle superiority |
| 100M+-class canonicalization policy A/B | B | `33265264254` | `9718634869`; SHA-256 `f3d084ca2a0c28aaa84c3124498da63266169608371cd633f5390337c94dde74` | `com-Orkut`, 234,370,166 directed arcs; 60 epochs; exactness gates every epoch | Support the storage-maintenance claim: higher maintenance-amortized throughput and less consolidation time at a documented RSS trade-off | One retained hosted execution; do not generalize to all hardware or graph families |

## Central adaptive-policy evidence

The adaptive-policy implementation and development ledger are strong enough to establish the paper's central experimental design:

- `always_full`
- `always_incremental`
- `simple_threshold`
- `adaptive`
- measured per-batch oracle
- exact full-BFS verification after each batch

The frozen selector development record in [`ablation-study.md`](ablation-study.md) reports 100% exactness, 3.148% mean oracle-relative regret, 19.780% p95 batch regret, and 18.253% worst-regime regret for the documented development campaign. The frozen benchmark source commit is `4381113005c221e6db2c19bac753a57d872e6374` and the recorded source blob is `c30f3459d22e61fb95abdb077aa2fd0eda445690`.

**Current status: Tier C for headline citation until the final GitHub Actions run/artifact corresponding to the manuscript-selected selector result is cross-referenced here.** The code path and metric ledger are usable for design/ablation discussion; the manuscript should not cite a headline adaptive number without its final retained run identity.

## Pending unified three-system campaign

The same-machine VeloGraphX / NetworKit / RisGraph campaign is intentionally **Tier C**. Its workflow covers web, directed social, road, controlled R-MAT, and larger-social graphs across seven update fractions from 0.0001% through 10%, but its numerical results must remain out of the manuscript until all matrix artifacts pass the documented exactness and provenance audit.

Until then:

1. use the accepted NetworKit and RisGraph campaigns above independently;
2. never merge their absolute times into a single league table; and
3. present only within-run paired ratios or explicitly labelled workload-specific comparisons.

## Manuscript result selection

The paper should prefer a small set of defensible results over a large benchmark catalogue.

### Primary figure — repair/recompute crossover

Use accepted adaptive/update-fraction evidence to show where localized exact repair is preferable, where full recomputation is preferable, and how the adaptive policy tracks the best available execution choice. The figure should report exactness and an oracle-relative metric in addition to latency.

### Primary external-baseline table

Use the accepted same-run evidence above. Keep winners and losses visible. At minimum, the table should communicate:

- NetworKit wins on `ca-GrQc` while VeloGraphX wins on the evaluated `web-Google` campaign;
- GAP wins weighted SSSP in the hosted static campaign while VeloGraphX wins the hosted BFS cases; and
- RisGraph wins the documented `web-Google` dynamic-BFS run even though VeloGraphX repair materially improves over its legacy full-recompute path.

### Supporting mechanism evidence

Use the GoldenCounter triangle comparison and Orkut canonicalization A/B as secondary evidence that the design extends beyond a single BFS result. They should not displace the adaptive repair-vs-recompute thesis.

## Statistical reporting policy

For manuscript tables and figures:

1. prefer median plus dispersion when raw repetitions are available;
2. retain raw per-repetition data and report the number of repetitions;
3. use paired comparisons when systems execute in one campaign under the same runner/timing envelope;
4. show confidence or bootstrap intervals for aggregate comparisons where the available raw samples support them;
5. keep correctness verification outside timed regions but require it before accepting a timing sample; and
6. preserve negative results rather than filtering to favorable datasets.

Historical documentation may report means. A manuscript figure should not silently convert a historical mean into a median; either derive the statistic from retained raw samples or label the statistic exactly as originally measured.

## Claims intentionally excluded from hosted evidence

The current paper does not need, and hosted runners should not be used to establish, the following claims:

- 8/16/32+ core scalability;
- true multi-socket NUMA locality or remote-memory superiority;
- stable hardware-counter improvements;
- AVX-512 or microarchitecture-specific universal speedups;
- research-scale NVMe / `io_uring` peak throughput;
- universal system-wide fastest-performance claims.

These are optional future evaluations. They are **not blockers** for a paper centered on exact adaptive execution, crossover, same-run relative comparisons, and reproducibility.

## Artifact-retention note

GitHub Actions artifacts are retention-limited. Run IDs, immutable revisions, hashes, and result documentation preserve provenance, but a submission-era artifact package should freeze the selected raw results outside ephemeral Actions retention (for example in a tagged release and DOI-capable archive). The paper should point reviewers to that frozen package rather than relying on old Actions ZIP availability.

## Source documents

- [Hosted native competitor evidence](hosted-native-competitors.md)
- [External dynamic baselines](external-dynamic-baselines.md)
- [Published exact triangle baseline](same-run-published-baseline.md)
- [Canonicalization A/B evidence](canonicalization-ab-evidence.md)
- [Ablation study](ablation-study.md)
- [Benchmark methodology](benchmark-methodology.md)
- [Paper claim-to-evidence map](paper-claims.md)
