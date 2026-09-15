# PVLDB 2027 build workspace

This directory turns the canonical `paper/manuscript.md` into a PDF using the **official PVLDB Volume 20 / VLDB 2027 template**.

## Official template pin

The workflow reads `template.lock` and checks out the official template repository at the exact pinned commit. The current pin is the official `vldbproceedings/VLDB-Template` revision recorded on September 15, 2026.

Do not copy an older VLDB template, use a local default `acmart.cls`, or edit the official class/style files.

## Source of truth

- Scientific prose: `../manuscript.md`
- Bibliography: `../references.bib`
- Audited result values: `../data/accepted-results.json`
- Selector regime values: `../data/current-selector-regimes.csv`
- Figure generator: `../figures/generate_figures.py`
- Venue schedule/gates: `../venue-plan.md`

`main.tex` is only the venue wrapper. `prepare_submission.py` extracts the abstract and body from the Markdown manuscript, normalizes section levels, and the CI workflow converts them to LaTeX with Pandoc.

## CI contract

`.github/workflows/pvlb-paper.yml`:

1. validates the committed manuscript data;
2. regenerates figures from audited data;
3. checks out the pinned official PVLDB template;
4. converts the canonical Markdown manuscript to LaTeX;
5. compiles the official-format PDF;
6. extracts the page containing `vldb-content-end` and fails if scientific content exceeds 12 pages; and
7. uploads the PDF and generated figures as a workflow artifact.

The page gate is intentionally strict: references may extend beyond page 12, but the scientific content may not.

## Before real submission

The current wrapper is a buildable submission-development scaffold, not permission to upload blindly. Before CMT submission:

- complete every author affiliation and coauthor entry;
- replace the temporary GitHub artifact URL with the DOI-capable archival artifact URL;
- inspect the compiled PDF against the official formatting guidelines;
- ensure every major table/figure is integrated in the manuscript at the correct location;
- verify bibliography metadata and citation coverage;
- run the external-review gate in `../submission-checklist.md`.

PVLDB is single-blind, so the final paper must include real author names and affiliations.
