# VeloGraphX paper outline

## Core thesis

VeloGraphX studies **when an exact dynamic graph engine should stop trying to repair locally and recompute from scratch**. The paper is centered on a pre-repair per-batch execution policy, not on claiming novelty for incremental graph processing or for the existence of an incremental/static switch.

1. **Motivation and problem definition**
   - Small graph changes can trigger either cheap localized repair or near-global work.
   - Incremental execution can become slower than fresh recomputation.
   - Define the online two-arm decision problem and the offline per-batch oracle.

2. **Prior art and claim boundary**
   - GraphIn: property-based incremental/static dual path.
   - Bok et al. 2022: history-based incremental/static cost model.
   - GraphBolt/KickStarter/Ingress: affected-region and dependency-driven incremental processing.
   - DZiG: adaptive incremental execution.
   - RisGraph: exact low-latency dynamic graph processing.
   - Layph: constraining change propagation.
   - State explicitly what VeloGraphX does **not** claim as novel.

3. **VeloGraphX system context**
   - Hybrid mutable storage: segmented CSR + packed deltas + sparse patches.
   - Forward/reverse adjacency and exact incremental algorithm state.
   - Keep storage detail sufficient to explain execution costs; avoid turning the paper into a storage survey.

4. **Localized exact repair and the crossover**
   - Repair semantics and correctness contract.
   - Why update fraction alone is insufficient.
   - Characterize graph scale, reachability/root state, affected work and update-type effects.

5. **Pre-repair repair-vs-recompute policy**
   - Observable pre-repair features.
   - Historical cost estimates and model freshness.
   - Uncertainty-aware choice.
   - Selector-owned recomputation.
   - Explicit avoidance of repair→full double work.

6. **Experimental methodology**
   - Same-run policy comparison and answer-ready timing boundary.
   - Frozen development/held-out discipline.
   - Exactness verification outside timing.
   - Dedicated hardware and reproducibility contract.

7. **Policy baselines and oracle**
   - Always full.
   - Always incremental.
   - Simple dual-path threshold heuristic.
   - History-cost reconstruction inspired by prior cost-model work.
   - Frozen VeloGraphX adaptive selector.
   - Offline per-batch oracle.

8. **Crossover experiments**
   - Multiple graph families, roots and update fractions.
   - Show where the winning arm changes.
   - Include negative regimes.

9. **Policy quality**
   - Mean/median/p95/p99/max oracle regret.
   - Wrong-arm rate.
   - Selector cost.
   - Full-recompute frequency.
   - Tail and worst-regime behavior.

10. **Double-work analysis**
    - Quantify repair discovery/work paid before fallback.
    - Compare internal fallback against pre-repair selector-owned recomputation.
    - Explain the final selector mechanism causally.

11. **Clean component ablation**
    - A0–A7 feature-controlled ablation on identical streams/hardware.
    - Keep historical development runs separate from causal ablation.

12. **Held-out generalization**
    - Frozen selector on unseen graph family/root/update regimes.
    - Report failure as well as success; no retuning on held-out data.

13. **External systems context**
    - Same-semantics native dynamic baselines where possible, including RisGraph/GraphBolt/DZiG-compatible evidence.
    - Do not mix incompatible timing envelopes into one synthetic ranking.

14. **Limitations and conclusion**
    - Policy is not claimed universal across all algorithms/hardware.
    - Hosted CI is engineering evidence, not publication hardware.
    - NUMA, compression, NVMe/out-of-core and Python packaging are supporting system capabilities, not the main novelty story.

No individual mechanism is labeled novel until the novelty ledger and experiments support the claim. In particular, do not claim invention of incremental-vs-static switching, history-based cost selection, affected-region repair, or adaptive incremental execution.