# Selector Tail-Regret Incident: web-Google Small-Batch Calibration

## Status

This note records a development finding discovered after the frozen `bounded-one-sided-warmup-v5` selector campaign. It is intentionally retained as evidence rather than removed from historical results.

The affected historical artifact remains reproducible at commit `4a0c3f3ffbeabe5e301dce386b5ccfdaa1366745`. The historical harness is not modified by the publication selector work.

## Observation

In the hosted single-root policy-ablation campaign on `web-Google`, root `481807`, batch size `512`, the adaptive selector showed a rare extreme per-batch oracle-regret outlier. The worst observed batch regret was approximately `54.24x` relative to the same-run oracle.

The broader multi-root campaign did not show a comparable regime-level collapse, which suggested a narrow selector-path issue rather than a general correctness or scaling failure.

## Root cause

The trace identifies the pathological decision as:

`large_one_sided_full`

At that point:

- the initial BFS had already measured a full-recompute cost baseline;
- no incremental cost sample had yet been collected;
- the selector nevertheless forced an additional full recomputation before allowing an incremental calibration probe;
- on a very small update fraction, full recomputation was dramatically slower than localized repair.

One representative historical batch measured roughly `62.8 ms` for the forced full path while the per-batch oracle was roughly `1.14 ms`.

This is a selector calibration defect, not a correctness defect. Exactness remained intact.

## Correction

The publication selector `publication-preflight-v1` removes the redundant one-sided full warmup.

For large graphs:

1. the initial BFS remains the measured full-cost baseline;
2. a shallow destructive first batch may still select full recomputation when warranted;
3. if the incremental arm is still unmeasured and the current batch is below the preflight-full threshold, the selector directly probes incremental execution;
4. subsequent decisions use the existing uncertainty-aware comparison.

The historical selector remains unchanged for provenance.

## Publication harness additions

The publication harness also adds:

- `history_cost_model`, explicitly described as a methodological reconstruction rather than an original prior-work implementation;
- a two-arm oracle defined only as `min(always_incremental, always_full)`;
- explicit pre-repair arm choice;
- wrong-arm audit support;
- internal repair-to-full fallback flags;
- fallback execution-time accounting;
- decision-cost and selector-setup timing;
- exactness checks outside the measured answer-ready interval.

## Evaluation discipline

Because this correction was motivated by the observed `web-Google` root `481807` small-batch result, that workload is now development evidence and must not be described as held out or unseen in a paper.

A future held-out generalization claim must use a newly frozen graph/root/update regime after `publication-preflight-v1` is frozen.

Hosted GitHub runner results remain engineering/development evidence (`research_claim=false`). Publication performance claims still require controlled dedicated hardware.
