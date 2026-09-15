# VeloGraphX paper outline

## Core thesis

VeloGraphX studies **when an exact dynamic graph engine should repair locally and when it should recompute from scratch**. The paper is centered on a pre-repair per-batch execution policy over two exact physical strategies, not on claiming novelty for incremental graph processing or for the existence of an incremental/static switch.

## 1. Motivation and problem definition

- Small graph changes can trigger either cheap localized repair or near-global work.
- Incremental execution can become slower than fresh recomputation.
- Define the online two-arm exact execution problem and the offline per-batch oracle.
- Explain why a win-only “incremental is faster” story would miss the systems problem.

## 2. Prior art and claim boundary

- Dynamic/incremental graph processing and affected-region execution.
- Systems that expose incremental/static or recomputation alternatives.
- History/cost-based execution selection.
- Dynamic graph systems including RisGraph, GraphBolt/DZiG-family work, and comparable exact primitives.
- Static performance context from GAP and GraphBLAS/LAGraph.
- State explicitly what VeloGraphX does **not** claim as novel.

No individual mechanism is labeled novel merely because VeloGraphX implements it. The contribution is the integrated exact execution architecture, its pre-repair policy, and the experimentally characterized crossover under a reproducible contract.

## 3. VeloGraphX system context

- Hybrid mutable storage: segmented CSR + packed deltas + sparse patches.
- Forward/reverse adjacency and exact incremental algorithm state.
- Explicit bounded canonical consolidation.
- Keep storage detail sufficient to explain execution costs; avoid turning the paper into a storage survey.

## 4. Localized exact repair and the crossover

- Repair semantics and independent exactness verification.
- Conservative recomputation fallback is part of the contract.
- Why update fraction alone is insufficient.
- Characterize graph scale, reachability/root state, affected work, and update-type effects.

## 5. Pre-repair repair-vs-recompute policy

- Observable pre-repair features.
- Historical cost estimates and model freshness.
- Large-graph preflight behavior.
- Selector-owned recomputation.
- Avoid repair→full double work when evidence already favors a global execution.
- Clearly identify the current manuscript selector from an audited artifact; keep historical selector-development versions labelled as historical.

## 6. Experimental methodology

- Same-run policy comparison and answer-ready timing boundary.
- Checksum-pinned datasets and competitor revisions.
- Repeated raw samples and median/dispersion reporting.
- Exactness verification outside timing.
- Negative-result retention.
- Hosted same-run/paired evidence supports scoped relative claims.
- Dedicated hardware is optional unless the paper makes many-core, NUMA, hardware-counter, storage-device-specific, or universal peak-throughput claims.

## 7. Policy baselines and oracle

- Always full.
- Always incremental.
- Simple threshold heuristic.
- Current publication/adaptive policy.
- History-cost baseline where supported by the publication harness.
- Offline exact oracle defined from compatible candidate strategies.

## 8. Crossover and policy quality — primary figure

- Show where the winning exact arm changes.
- Keep graph/update regimes visible rather than reporting one aggregate average.
- Report exactness, oracle-relative regret, tail regret, wrong-arm or internal-fallback diagnostics, selector cost, and full-recompute frequency where available.
- Include negative regimes.

## 9. External systems context — primary table

- Paired NetworKit `DynBFS`: VeloGraphX wins the evaluated `web-Google` workload; NetworKit wins `ca-GrQc`; 30/30 paired executions exact.
- Same-run GAP/LAGraph: VeloGraphX wins the evaluated BFS cases; GAP substantially wins weighted SSSP.
- RisGraph: retain the workload where RisGraph remains faster.
- Never combine absolute timings from separate hosted runs into a synthetic three-system ranking.

## 10. Exact dynamic triangle breadth — supporting figure

- Same-run published exact GoldenCounter reference.
- 15/15 exact comparisons on `facebook-combined`.
- Report semantically fair exact-answer-ready timing, not update-only timing for a reference whose exact answer still requires a query.
- Optional multi-dataset triangle crossover as supporting graph-dependent crossover evidence.

## 11. Large-graph storage maintenance — supporting figure

- `com-Orkut`, 234M directed arcs, 60 epochs.
- Bounded 1.25× vs 1.50× storage envelope.
- Show consolidation reduction, 2.25× maintenance-amortized throughput, and +6.6% peak-RSS trade-off.
- Do not claim the threshold is universally optimal.

## 12. Clean ablation boundary

- Historical mechanism-development runs motivate the design but are not a cumulative causal A0–A7 experiment.
- Use clean policy-level comparisons from one harness for claims about adaptive selection as a whole.
- If the paper makes causal claims about individual selector mechanisms, add explicit feature switches and a frozen component-ablation experiment; otherwise keep the mechanism discussion descriptive.

## 13. Limitations

- Policy is not claimed universal across all algorithms or hardware.
- NetworKit evidence currently covers one thread and two graph families.
- Hosted 1–4-thread scaling is not many-core/NUMA evidence.
- Weighted destructive updates may conservatively recompute.
- Python/API packaging is system maturity evidence, not paper novelty.

## 14. Conclusion

The paper closes on a systems principle rather than a leaderboard claim:

> **Incremental maintenance should be a selectable exact execution strategy, not an unconditional architectural assumption.**
