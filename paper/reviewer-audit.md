# VeloGraphX reviewer-style submission audit

This is an internal pre-submission review of **VeloGraphX: Adaptive Exact Analytics for Evolving Graphs**. It is intentionally stricter than the repository README and is meant to surface plausible reject reasons before a conference reviewer does.

## Overall assessment

**Current paper potential:** strong top-tier database/systems submission if the manuscript remains narrow, preserves the negative held-out result, and completes the final presentation/archive gates.

| Dimension | Current assessment | Main reason |
| --- | --- | --- |
| Problem importance | Strong | Exact analytics on evolving graphs face a real repair-vs-recompute crossover. |
| Systems novelty | Strong but positioning-sensitive | The contribution is the integrated pre-repair exact physical-plan choice over one mutable substrate, not the generic idea of incremental/static switching. |
| Technical depth | Strong | Mutable storage, exact BFS repair, explicit fallback, pre-repair policy, correctness contracts, and plan telemetry are implemented. |
| Experimental credibility | Very strong | Primary oracle-relative campaign, production 0.35 fallback replay, frozen no-retuning holdout, clean feature ablation, matched GraphBolt evidence, exactness gates, and retained negative results. |
| Reproducibility | Very strong | Reviewer-facing artifact map, machine-readable registries, retained run/artifact identifiers and digests, reproduction workflows, submission-freeze metadata, and deterministic figure inputs. |
| External dynamic baselines | Stronger than before | Matched GraphBolt evidence now complements NetworKit and RisGraph; workload-specific reversals are retained. |
| Generalization evidence | Mixed by design | `Amazon0312` is strong; timestamp-ordered `CollegeMsg` is a substantial negative result and prevents a universal selector claim. |
| Hardware breadth | Limited by design | Hosted evidence is adequate for the scoped central claim but not for many-core/NUMA/microarchitecture claims. |
| Manuscript readiness | Strong synchronized draft | Scientific prose now includes the closure experiments; remaining work is figures/tables, citation polish, archival DOI, and external review. |

## Likely reviewer objections and required responses

### 1. “Incremental-vs-static switching already exists.”

**Severity:** high if framed poorly; low if framed correctly.

GraphIn already uses dual incremental/static execution, and Bok et al. already use historical cost information to choose incremental or static processing. VeloGraphX must avoid first-of-kind language about switching or cost-based selection.

**Required paper response:** define the contribution as the combination of exact localized repair and full recomputation as physical plans over one mutable state, a decision made before expensive repair discovery when possible, explicit separation of selector and fallback, direct measurement of repair-then-full work, oracle/regret evaluation, and an artifact discipline that preserves failures.

**Status:** addressed in the synchronized manuscript and related-work notes. Keep citations precise.

### 2. “The current selector does not generalize uniformly.”

**Severity:** high if hidden; medium if presented as a scoped policy limitation.

The frozen held-out campaign is exact with no post-result retuning, but the two held-out families differ sharply. `Amazon0312` records 1.52% equal-regime mean regret, while timestamp-ordered `CollegeMsg` records 34.52%, with a 185.83% worst-regime mean and 311.88% worst-regime p95.

**Required paper response:** make the architectural claim primary and the current selector claim secondary. State explicitly that the physical-plan abstraction generalizes more broadly than the present hand-designed policy. Do not retune thresholds after seeing `CollegeMsg` merely to improve the reported result.

**Status:** addressed in abstract, evaluation, discussion, limitations, and conclusion.

### 3. “You motivate avoiding repair→full double work, but is it directly measured?”

**Severity:** now low.

The production 0.35 campaign directly compares fallback-only behavior with the same frozen selector path. Across 93 exact aligned observations, fallback-only falls back six times; the pre-repair selector avoids all six opportunities and 17.323 ms of conservatively measured double work, while retaining 33 false-full choices.

**Required paper response:** keep both the benefit and the cost visible. Do not generalize the 220K cascade stress case to natural-workload frequency.

**Status:** closed.

### 4. “Which selector mechanisms actually matter?”

**Severity:** now low-to-medium.

The clean one-factor-at-a-time ablation shows that removing structural preflight worsens equal-regime mean regret from 4.82% to 8.16%, while removing uncertainty raises it to 142.97% with a catastrophic tail. Removing previous-affected-work slightly improves the aggregate result to 4.20%.

**Required paper response:** claim support for structural preflight and uncertainty, but do not claim that every existing feature is independently beneficial. Treat previous-affected-work as unresolved or a candidate for simplification.

**Status:** closed scientifically; retain the neutral/negative result.

### 5. “Hosted runners are noisy.”

**Severity:** medium if making absolute peak-performance claims; low for the current claim scope.

**Required paper response:** emphasize same-run/paired comparisons, exact timing envelopes, repetitions, dimensionless ratios where possible, and explicit exclusion of many-core/NUMA/hardware-counter claims.

**Status:** addressed in methodology and limitations.

### 6. “Why only BFS for the adaptive policy?”

**Severity:** medium.

**Required paper response:** BFS is the controlled vehicle because it exposes insertion/deletion dependency changes and admits a direct exact recomputation oracle. Use triangles, static context, and storage as breadth evidence without implying one selector transfers unchanged to every analytic. Keep PageRank outside the exactness claim by using residual/tolerance-validation wording.

**Status:** addressed.

### 7. “External-system results look cherry-picked.”

**Severity:** low-to-medium if the reversals remain visible.

The evidence now includes matched GraphBolt, paired NetworKit, separate RisGraph, and static GAP/LAGraph context. VeloGraphX wins some regimes and loses others. The GraphBolt ratio reverses by update fraction; NetworKit wins `ca-GrQc`; GAP wins weighted SSSP; RisGraph wins its documented hosted campaign.

**Required paper response:** preserve each workload-specific reversal and never merge unrelated absolute times into one league table.

**Status:** addressed.

### 8. “The paper has too many secondary stories.”

**Severity:** high if not controlled.

The repository contains storage, compression, multicore, Python packaging, several algorithms, and many benchmark campaigns.

**Required paper hierarchy:**

1. exact repair/recompute crossover and physical-plan architecture;
2. primary selector behavior;
3. production fallback double-work evidence;
4. frozen held-out generalization and clean ablation;
5. matched external dynamic baselines;
6. triangle/storage breadth only as supporting evidence.

**Status:** synchronized manuscript follows this hierarchy; preserve it during figure/table integration.

## Internal readiness scores

These are internal manuscript-readiness scores, **not acceptance probabilities**.

| Review category | Score / 10 |
| --- | ---: |
| Significance | 9.1 |
| Technical quality | 9.4 |
| Novelty after proper positioning | 8.8 |
| Evaluation | 9.5 |
| Reproducibility | 9.8 |
| Clarity potential | 9.3 |
| Artifact quality | 9.8 |
| Overall submission readiness | 9.4 |

The main reason the score is not higher is no longer missing experimentation. It is final-paper execution: visual evidence integration, citation quality, archival permanence, and independent external review.

## Must close before submission

1. Integrate final figures/tables for the primary selector, production fallback, held-out result, ablation, and external baseline summary at conference-readable size.
2. Finalize dataset/workload and hardware/software environment tables.
3. Complete the bibliography/citation audit, especially GraphIn, Bok et al., GraphBolt/DZiG, RisGraph, Layph, GraphOne, Teseo, GAP, LAGraph, and NetworKit.
4. Ensure every abstract number maps visibly to a figure/table or obvious manuscript evidence row.
5. Freeze the exact submission commit and selected raw evidence in a durable release and mint/verify the DOI-capable archive record.
6. Perform one independent database/graph-systems review focused on novelty, baselines, and the interpretation of the `CollegeMsg` holdout.
7. Complete author/affiliation/ORCID/COI metadata and the current PVLDB venue-specific disclosure requirements.
8. Build and visually inspect the final official-format PDF after the synchronized manuscript and figures are integrated.

## Explicit non-blockers

The following are **not required** unless the claim scope changes:

- buying dedicated hardware;
- 32/64-core scalability;
- multi-socket NUMA experiments;
- AVX-512 superiority;
- adding more graph algorithms;
- retuning the selector to eliminate `CollegeMsg` or `web-Google` tails;
- producing a universal fastest-system league table.

## Submission gate

Core experimentation is now substantially closed. The paper should remain in manuscript/figure/archive/external-review mode unless a reviewer finds a concrete correctness, baseline, or methodology gap. Do not reopen broad engineering development merely to chase a cleaner headline number.
