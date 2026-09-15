# VeloGraphX manuscript workspace

This directory contains the manuscript-facing material for the VeloGraphX database-systems paper.

## Working title

**VeloGraphX: Adaptive Exact Analytics for Evolving Graphs**

## Primary thesis

The paper studies a systems question rather than claiming that incremental graph processing always wins:

> For exact analytics on evolving graphs, the preferred execution strategy changes with graph structure and update regime; a dynamic graph engine should therefore expose both localized repair and full recomputation and adapt between them.

The paper should remain centered on this thesis. Storage, additional algorithms, multicore execution, compression, and external baselines are supporting system evidence rather than independent headline stories.

## Submission target

The primary target is now **PVLDB Volume 20 / VLDB 2027, Regular Research Paper, December 1 2026 cycle**. The mandatory abstract deadline is November 25, 2026.

- [`venue-plan.md`](venue-plan.md) — venue decision, deadlines, freeze policy, and submission gates.
- [`vldb/`](vldb/) — official-template build wrapper pinned to the current PVLDB Volume 20 template revision.
- [`.github/workflows/pvldb-paper.yml`](../.github/workflows/pvldb-paper.yml) — reproducible official-format PDF build and 12-page content gate.

## Files

- [`manuscript.md`](manuscript.md) — working full-paper draft and section-level argument.
- [`results-ledger.md`](results-ledger.md) — figure/table plan tied to retained runs and claim boundaries.
- [`reviewer-audit.md`](reviewer-audit.md) — strict pre-submission reviewer simulation and likely reject reasons.
- [`submission-checklist.md`](submission-checklist.md) — scientific, artifact, figure, and venue-finalization gate.
- [`related-work-notes.md`](related-work-notes.md) — claim-by-claim novelty boundary and prior-work notes.
- [`references.bib`](references.bib) — manuscript bibliography seed.
- [`data/accepted-results.json`](data/accepted-results.json) — compact machine-readable values selected for manuscript construction.
- [`figures/generate_figures.py`](figures/generate_figures.py) — deterministic figure generator reading only committed paper evidence.
- [`figures/README.md`](figures/README.md) — figure/caption mapping and final visual-QA rules.
- [`validate_submission_data.py`](validate_submission_data.py) — standard-library consistency check for paper evidence inputs.
- [`../docs/paper-evidence-index.md`](../docs/paper-evidence-index.md) — authoritative repository-wide evidence registry.
- [`../PAPER.md`](../PAPER.md) — reviewer-facing artifact guide.

## Evidence rules

1. Every quantitative manuscript statement must map to a retained artifact, a repository document that records its provenance, or a fresh audited publication-selector artifact.
2. Same-run and paired GitHub-hosted experiments may support narrowly scoped relative claims. They do not establish universal peak performance.
3. Absolute timings from different hosted runners must never be combined into a synthetic cross-system ranking.
4. Exactness gates are mandatory for dynamic results.
5. Negative results stay visible: GAP, NetworKit, RisGraph, and full recomputation are allowed to win where the retained experiments show that they do.
6. Many-core, multi-socket NUMA, hardware-counter, and storage-device-specific claims require suitable controlled hardware and are outside the default manuscript scope.
7. Historical selector-development numbers must be labelled as development evidence unless the corresponding retained run/artifact is explicitly audited for the submitted manuscript.

## Local submission-data validation

Run from the repository root:

```bash
python3 paper/validate_submission_data.py
```

The same validation is part of the Publication Artifact Contract workflow so accidental drift between the paper-facing CSV and the audited JSON registry fails CI.

## Engineering freeze

Core engineering is frozen unless an external reviewer identifies a concrete scientific gap. The priority order is now: official-format manuscript, figures/tables, citations, external review, archival artifact freeze, then CMT submission QA.
