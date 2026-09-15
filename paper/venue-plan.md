# Venue decision and submission plan

## Primary target

**PVLDB Volume 20 / VLDB 2027 — Regular Research Paper**

Target cycle: **December 1, 2026** paper deadline, with the mandatory abstract submitted by **November 25, 2026**.

Official submission and formatting sources:

- https://www.vldb.org/2027/submission-guidelines.html
- https://www.vldb.org/2027/formatting-guidelines.html
- https://vldb.org/2027/call-for-research-track.html
- Official template: https://github.com/vldbproceedings/VLDB-Template

The paper should be submitted as a **Regular Research Paper**, not Experiment/Analysis/Benchmark. The primary contribution is a new system design and adaptive exact execution policy; the experimental package supports that contribution rather than constituting the paper by itself.

## Why this target

PVLDB is the strongest fit for the current state of VeloGraphX because:

1. the contribution is a data-management/systems design for evolving graph analytics;
2. the current manuscript and evidence package are already close to the 12-page regular-research format;
3. PVLDB explicitly expects supplementary code/data/artifacts, which is a VeloGraphX strength;
4. PVLDB permits one revision, unlike ICDE 2027's accept/reject-only research-track process; and
5. the December cycle leaves enough time for independent expert review, final figures, archival artifact freezing, and formatting QA without reopening core engineering.

## Alternatives intentionally not chosen

### SIGMOD 2027 Round 4

Abstract/COI deadline: October 10, 2026. Paper deadline: October 17, 2026.

This is technically possible but unnecessarily aggressive for the current manuscript-polish stage. The remaining risk is novelty presentation and reviewer-facing clarity, not missing implementation. Rushing those aspects would reduce the value of the current evidence package.

### ICDE 2027 Round 2

Paper deadline: November 11, 2026.

ICDE is an excellent topical fit, but its 2027 research track provides Accept/Reject decisions without a revision option. PVLDB therefore offers a better risk-adjusted path for this system paper.

## Submission milestones

### By October 5

- official PVLDB template builds in CI;
- all main figures and tables generated from committed audited data;
- paper body converted into the official template;
- every related-work claim has a real citation;
- no quantitative result exists only in prose.

### By October 20

- first complete 12-page draft;
- independent database/graph-systems review requested from at least two qualified researchers;
- reviewer questions logged and triaged;
- no new features unless a review identifies a true scientific gap.

### By November 5

- second full draft after external review;
- artifact reproduction tested from a clean environment;
- submission figures checked at printed two-column size;
- all claimed baselines and losses retained.

### By November 15

- freeze the manuscript-selected code/results revision;
- create a tagged release for the submission artifact;
- deposit the artifact in a DOI-capable archival repository;
- replace the temporary artifact URL in the PVLDB top matter with the archival URL.

### November 25

- submit mandatory abstract and all author metadata in CMT;
- verify all author accounts and nominated reviewer information;
- re-check related/concurrent-submission declarations.

### December 1

- submit the paper and supplemental artifact before 5:00 PM Pacific Time.

## Hard submission gates

Do not submit until all of the following are true:

- official current PVLDB template is used without modifying `acmart.cls` or `pvldb.sty`;
- content page count is at most 12 pages excluding references;
- author names and affiliations are complete (PVLDB is single-blind);
- artifact URL points to a durable public archive rather than an ephemeral Actions artifact;
- every headline number maps to `paper/data/accepted-results.json` or an explicitly audited retained artifact;
- the visible `web-Google` selector tail remains in the paper;
- competitor wins remain visible;
- the unified three-system campaign is excluded unless separately audited;
- no universal-fastest, many-core, NUMA, NVMe, or microarchitecture-specific claims are introduced;
- at least one external database/graph-systems researcher has read the complete draft.

## Freeze policy

From this point onward, `main` is the engineering baseline. New code or benchmark campaigns require a concrete manuscript/reviewer justification. Formatting, citations, figure QA, artifact packaging, and reviewer-response work have priority over feature development.
