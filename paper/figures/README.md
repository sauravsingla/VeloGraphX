# Manuscript figures

The paper figures in this directory are generated only from committed evidence under `paper/data/`.

## Generate

From the repository root:

```bash
python3 paper/figures/generate_figures.py
```

The script writes PDF and PNG files under `paper/figures/generated/`.

## Evidence rule

Do not hand-edit numerical values in a figure. If a manuscript number changes, update the audited evidence registry first, then the committed paper data, then regenerate the figure.

## Planned figure/table map

### Figure 1 — system architecture

Not data-derived. Draw the logical path:

`update batch -> mutable graph substrate -> pre-repair selector -> localized repair OR full recomputation -> exact result`

Show bounded storage consolidation as a separate maintenance path, not as a third analytic arm.

### Figure 2 — current selector quality

Generated from `paper/data/current-selector-regimes.csv`.

Required caption facts:

- three graph families;
- three regimes per graph;
- five repetitions per regime;
- 1,610 adaptive batch samples overall;
- all outputs exact;
- mean and p95 oracle-relative regret shown;
- the large `web-Google` tail is intentionally retained.

### Figure/Table 3 — exact dynamic triangles

Generated from `paper/data/accepted-results.json`.

Required caption facts:

- exact GoldenCounter reference component;
- five paired repetitions per update fraction;
- 15/15 exact;
- answer-ready timing boundary;
- no claim against the approximate SWTC algorithm.

### Figure/Table 4 — `com-Orkut` canonicalization A/B

Generated from `paper/data/accepted-results.json`.

Required caption facts:

- 234,370,166 directed arcs;
- 60 epochs;
- exactness checks preserved;
- 1.25x vs 1.50x bounded storage envelope;
- performance improvement shown together with the peak-RSS cost.

## Final visual QA

Before submission, inspect every figure at the actual conference column width and verify:

- axis labels remain legible;
- line/bar patterns remain distinguishable in grayscale;
- no legend covers data;
- no title duplicates the caption unnecessarily;
- units are explicit;
- percentages and ratios are not confused;
- error bars/dispersion are shown when the committed data support them.
