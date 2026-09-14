# Repair-vs-Recompute Policy Evaluation Contract

This document defines the publication experiment for VeloGraphX's candidate **pre-repair repair-vs-recompute execution-policy** contribution.

The purpose of this experiment is **not** to show that incremental processing can outperform recomputation. That is established prior art. The purpose is to test whether an online runtime policy can choose the cheaper exact execution arm early enough to avoid wasted repair work, while staying close to an offline oracle across graph families and update regimes.

## Research question

For an exact dynamic graph algorithm and an arriving update batch, can the runtime predict whether localized repair or fresh full recomputation will minimize answer-ready latency **before** expensive repair discovery becomes sunk cost?

Formally, for batch `b`, let:

- `T_inc(b)` = answer-ready latency when the batch is handled by localized exact repair;
- `T_full(b)` = answer-ready latency when the same batch is handled by fresh full recomputation;
- `T_oracle(b) = min(T_inc(b), T_full(b))`.

A policy `P` chooses one arm before execution and incurs `T_P(b)`, including selector decision cost. Per-batch non-negative regret is:

`R_P(b) = max(0, (T_P(b) - T_oracle(b)) / T_oracle(b))`.

The oracle is offline and is used only for evaluation.

## Claim boundary

Prior work already includes:

- property-based incremental/static switching (GraphIn);
- history-based cost selection between incremental and static processing (Bok et al. 2022);
- adaptive switching among incremental-processing strategies (DZiG);
- dependency/affected-region incremental processing (GraphBolt, KickStarter, Ingress and others);
- low-latency exact dynamic graph processing (RisGraph).

Therefore this experiment must **not** be described as proving the novelty of incremental-vs-static switching itself.

The candidate contribution being tested is the combination of:

1. **pre-repair** execution-arm selection;
2. graph/update/algorithm-state/history signals rather than update fraction alone;
3. uncertainty-aware decision logic;
4. selector-owned full recomputation that prevents hidden repair→full double work;
5. explicit per-batch oracle-regret evaluation including tail and worst-regime behavior;
6. exact outputs under every policy.

## Policies

Every campaign must run the following policies on the **same graph state, query/root and deterministic update stream**.

### P0 — `always_full`

Apply every batch and recompute the algorithm from scratch.

Purpose: lower-complexity static baseline and one oracle arm.

### P1 — `always_incremental`

Attempt localized exact repair for every batch without policy intervention.

Purpose: pure incremental baseline and the other oracle arm. If the underlying algorithm has an internal correctness-preserving emergency fallback, the artifact must report that event separately so it is not confused with a policy decision.

### P2 — `simple_threshold`

Choose full recomputation when update fraction exceeds one fixed threshold; otherwise choose incremental repair.

Purpose: simple dual-path heuristic baseline.

**Do not label this implementation “GraphIn”.** GraphIn is prior art motivating this class of baseline, but this threshold is not claimed to reproduce GraphIn's source or exact property model.

The threshold must be frozen before held-out evaluation and recorded in artifacts.

### P3 — `history_cost_model`

Use only historical cost/recalculation statistics available before the current batch to predict incremental versus full processing cost.

Purpose: methodological baseline inspired by prior history-based cost selection, especially Bok et al. 2022.

Unless the original authors' implementation is executed, the paper must call this a **reconstruction** or **history-cost baseline**, not “Bok et al.'s implementation”. Document the mapping from their variables/idea to the VeloGraphX harness.

This policy must not use VeloGraphX-specific uncertainty guards, scale guards, root-state features or selector-owned double-work avoidance beyond what is required for correctness.

### P4 — `adaptive`

The frozen VeloGraphX selector.

The policy may use only information available before committing to the current batch's expensive repair execution. Permitted signals must be enumerated explicitly in the artifact schema, for example:

- update fraction / batch size;
- graph vertex/edge scale;
- source/root reachability state;
- update type statistics observable from the batch;
- prior affected-work fraction;
- historical incremental/full execution cost;
- model age/freshness;
- historical prediction error / uncertainty.

Decision cost must be included in answer-ready policy timing.

### P5 — offline oracle

For evaluation only:

`oracle(b) = min(T_inc(b), T_full(b))`.

The oracle is not an executable online policy and must not influence decisions during a measured run.

## Required metrics

For each policy and regime report:

- exactness rate;
- mean and median answer-ready batch latency;
- p95 and p99 batch latency;
- full-recompute count/fraction;
- selector decision cost and one-time setup cost;
- affected vertices/edges where applicable;
- mean oracle regret;
- median oracle regret;
- p95 oracle regret;
- p99 oracle regret;
- maximum per-batch regret;
- wrong-arm count/fraction relative to the two-arm oracle;
- worst-regime mean regret;
- number/fraction of repair→full fallback events;
- latency attributable to repair work paid before a full fallback where measurable.

Aggregate results must include confidence intervals or bootstrap intervals for principal comparisons on publication hardware.

## Wrong-arm definition

Let the oracle arm for batch `b` be `incremental` if `T_inc(b) < T_full(b)` and `full` otherwise. A policy makes a wrong-arm decision when its explicit pre-repair choice differs from that oracle arm.

Ties must be handled deterministically and documented.

Wrong-arm rate complements regret: a policy may make many harmless near-tie mistakes or a small number of catastrophic mistakes. Report both.

## Double-work definition

A **double-work event** occurs when the runtime begins an incremental path, pays non-trivial affected-region discovery/repair work, and then performs a fresh full recomputation before producing the answer.

The artifact should distinguish:

- policy-selected full recomputation before repair begins;
- incremental completion;
- incremental execution that internally falls back to full recomputation;
- correctness/error fallback if such a path exists.

The paper should measure whether pre-repair policy ownership reduces the third category.

## Dataset and workload contract

Use structurally different graph families. At minimum publication evaluation should include:

- web / scale-free graph;
- social/community graph;
- low-degree road graph;
- synthetic R-MAT/Kronecker scalability family;
- at least one dataset held out from selector design.

Dynamic workloads must include insertions and deletions where supported. Evaluate across very small through disruptive batch fractions; the standard benchmark methodology's fraction grid should be reused where practical.

For BFS/SSSP-like algorithms use multiple roots spanning different reachability/locality regimes. A single favorable root is not sufficient evidence.

Dataset identity, normalization, root/query identity, update-stream seed/hash and exact changed-edge count must be retained.

## Development / held-out discipline

The selector and every threshold must be frozen before held-out evaluation.

A dataset/root/update regime used to tune thresholds, add guards, modify features or select model structure is a **development** workload and cannot later be presented as unseen.

Any selector redesign after observing the held-out result requires a newly preregistered held-out workload for a fresh generalization claim.

## Same-run fairness

For direct policy comparison use:

- identical VeloGraphX commit;
- identical compiler and flags;
- identical physical graph representation at the start of each policy run;
- identical root/query;
- identical update order and batch boundaries;
- identical thread count, affinity and NUMA placement;
- identical warmup and repetition protocol;
- identical answer-ready timing boundary;
- correctness validation outside the timed region unless explicitly stated otherwise.

Do not create a synthetic ranking from timings collected on different machines.

## Publication hardware

Hosted GitHub runners are suitable for CI correctness, smoke tests and engineering evidence but are not sufficient for the main performance claim.

The publication campaign must use controlled dedicated hardware and record CPU model, sockets/NUMA topology, RAM, kernel, compiler, governor/frequency policy, affinity, thread count and major runtime settings.

## Required figures

### Figure A — crossover curve

X-axis: update fraction or measured affected-work proxy.

Y-axis: answer-ready latency.

Show `always_incremental`, `always_full`, `simple_threshold`, `history_cost_model`, `adaptive`, and oracle.

The intended visual question is whether `adaptive` stays near the lower envelope as the winning arm changes.

### Figure B — regret distribution

CDF or complementary CDF of per-batch oracle regret for each online policy.

Highlight p95, p99 and maximum regret.

### Figure C — decision failure anatomy

Break non-oracle overhead into:

- selector cost;
- wrong-arm cost;
- repair discovery/work before fallback;
- full recomputation cost.

This figure directly tests the “pre-repair selection avoids sunk repair work” hypothesis.

### Figure D — held-out generalization

Report the frozen selector on an unseen graph family/root regime with no retuning.

## Acceptance gates for the candidate policy claim

These are research-development gates, not universal claims:

- exactness: `100%`;
- mean oracle regret: `<= 5%` on the preregistered development suite;
- p95 oracle regret: `<= 20%`;
- worst-regime mean regret: `<= 25%`;
- selector decision cost reported explicitly;
- held-out result reported regardless of outcome;
- no hidden repair→full fallback inside an aggregate policy latency metric.

Failure of a gate is evidence, not a reason to delete a workload.

## Baseline reconstruction disclosure

For reconstructed prior-art-inspired baselines, record:

- paper citation;
- exact idea being reconstructed;
- parts that cannot be reproduced from the publication;
- VeloGraphX-specific mapping choices;
- parameters and how they were frozen;
- whether original source code was available and, if available, why it was or was not used.

A reconstructed baseline must never be presented as an author's original implementation.

## Artifact schema additions

Policy result artifacts should eventually contain fields equivalent to:

```json
{
  "policy": "adaptive",
  "explicit_pre_repair_choice": "incremental",
  "oracle_arm": "full",
  "wrong_arm": true,
  "decision_us": 0.0,
  "execution_us": 0.0,
  "answer_ready_us": 0.0,
  "oracle_us": 0.0,
  "regret": 0.0,
  "internal_full_fallback": false,
  "repair_work_before_fallback_us": 0.0,
  "exact": true
}
```

The exact schema may differ, but the information required to audit policy decisions must be retained.

## Paper wording if the experiment succeeds

A safe contribution statement is:

> We study exact dynamic graph execution as an online repair-versus-recompute policy problem. VeloGraphX makes the decision before expensive localized repair becomes sunk cost, combines observable graph/update/state/history signals with uncertainty-aware cost estimates, and gives the selector ownership of recomputation to prevent repair→full double work. We evaluate decision quality against a same-run per-batch oracle and report both average and tail regret.

Do not add “first” unless a completed literature review supports it.