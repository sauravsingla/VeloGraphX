# VeloGraphX submission checklist

This checklist freezes the transition from engineering/research development to a conference submission package. It is intentionally conservative: an unchecked box means a real remaining task rather than a forgotten status update.

## A. Scientific scope freeze

- [x] Central thesis fixed: exact localized repair and full recomputation are competing execution strategies whose preferred choice changes by graph/update regime.
- [x] Current publication selector identified and audited.
- [x] Negative selector tail retained rather than tuned away.
- [x] Frozen held-out selector campaign executed with no post-result retuning.
- [x] Timestamp-ordered `CollegeMsg` negative result retained and used to narrow the generalization claim.
- [x] Production 0.35 fallback experiment directly measures repair→full work avoided.
- [x] Clean one-factor-at-a-time selector feature ablation retained.
- [x] External competitor wins and winner reversals retained.
- [x] Dedicated hardware excluded as a requirement for the core claim.
- [x] Many-core/NUMA/NVMe/microarchitecture claims excluded from manuscript scope.
- [x] No new core feature is merged without a paper-critical justification.

## B. Novelty and related work

- [x] GraphIn recognized as prior dual incremental/static execution.
- [x] Bok et al. recognized as prior history/cost-based incremental/static selection.
- [x] GraphBolt and DZiG recognized as dependency/sparsity-aware incremental processing.
- [x] RisGraph recognized as prior low-latency evolving-graph analytics.
- [x] Layph recognized as prior work on constraining broad change propagation.
- [x] GraphOne included in mutable-storage positioning.
- [x] Teseo included in mutable-storage positioning.
- [x] Related Work is integrated once and avoids “first to” novelty claims already covered by prior work.
- [ ] One independent DB/graph-systems researcher reads the novelty, baselines, and methodology before submission.

## C. Evidence and statistics

- [x] Current selector evidence has retained run/artifact IDs and digest.
- [x] Current selector exactness, mean regret, weighted regret, wrong-arm rate, decision overhead, and tail are recorded.
- [x] Production 0.35 fallback evidence has retained run/artifact ID, digest, exactness, fallback counts, false-full count, and measured avoided double work.
- [x] Frozen held-out evidence has retained run/artifact ID, digest, exactness, no-retuning declaration, `Amazon0312` result, and `CollegeMsg` negative result.
- [x] Clean feature ablation has retained run/artifact ID, digest, exactness, structural/uncertainty results, and the neutral/negative previous-affected-work result.
- [x] Matched GraphBolt comparison retained with exactness/verification and winner reversal.
- [x] NetworKit paired campaign retained with exactness.
- [x] GAP/LAGraph static context retained.
- [x] RisGraph comparison retained separately.
- [x] Published exact triangle reference comparison retained.
- [x] Orkut canonicalization A/B retained.
- [x] Multi-dataset triangle crossover retained as supporting evidence.
- [x] Cross-run absolute timing league tables prohibited.
- [x] Every quantitative statement added to the synchronized manuscript maps to `data/accepted-results.json`, `submission-closure-evidence.json`, or the results ledger.
- [ ] Final figures show repetitions/dispersion where supported.
- [ ] Dataset/workload table finalized in the paper PDF.
- [ ] Hardware/software/environment table finalized in the paper PDF.

## D. Figures and tables

Required final paper surfaces:

- [ ] **Figure 1:** system architecture / execution path.
- [ ] **Figure 2:** primary selector regret/crossover across graph/regime.
- [ ] **Figure/Table 3:** production 0.35 fallback double-work evidence.
- [ ] **Figure/Table 4:** frozen held-out (`Amazon0312` + timestamp-ordered `CollegeMsg`) result.
- [ ] **Table 1:** datasets, graph family, vertices/edges, update regime, roots/repetitions.
- [ ] **Table 2:** external baseline summary including GraphBolt and workload-specific wins/losses.
- [ ] Supporting figure/table: clean selector feature ablation.
- [ ] Supporting figure/table: exact dynamic triangle published-reference result.
- [ ] Supporting figure/table: Orkut canonicalization A/B memory/performance trade-off.
- [ ] All figures readable at final conference column width and in grayscale/print.
- [ ] Captions state timing boundary, repetitions, exactness, and scope where relevant.

## E. Manuscript quality

- [x] Abstract synchronized with the current evidence and narrowed to the central architectural thesis.
- [x] Introduction ends with a crisp problem gap and four contributions.
- [x] Related Work is integrated once, without duplicated “prior art” and “related work” sections.
- [x] System design explains the pre-repair decision point and repair-then-full double-work risk.
- [x] Experimental methodology states hosted-runner scope before results.
- [x] Primary historical graph/root program is explicitly distinguished from the frozen held-out campaign.
- [x] Evaluation follows the central-story hierarchy rather than repository feature breadth.
- [x] Production fallback, frozen held-out, clean ablation, and GraphBolt evidence are in the canonical manuscript.
- [x] Limitations explicitly include the historical-root validation, large-`web-Google` tail, `CollegeMsg` held-out failure, temporal-semantics caveat, fallback stress-case boundary, and GraphBolt scope.
- [x] PageRank wording is residual/tolerance validated with conservative fallback rather than folded into the exact BFS claim.
- [x] Conclusion states a systems lesson, not a universal fastest-system or universal-selector claim.
- [x] Terminology is consistent: `localized repair`, `full recomputation`, `pre-repair selector`, `oracle regret`, `wrong-arm`, `internal fallback`.

## F. Artifact and archival package

- [x] Reviewer-facing `PAPER.md` exists.
- [x] Minimal reproduction command exists.
- [x] Evidence registry exists in human-readable and JSON form.
- [x] Current selector manual reproduction workflow exists.
- [x] Deterministic figure generator reads committed paper evidence.
- [x] Submission-freeze metadata and Zenodo metadata are present.
- [x] Release workflow for the intended submission tag is present.
- [ ] Freeze selected raw artifacts outside GitHub Actions retention.
- [ ] Create the final submission tag/release from the exact submitted commit.
- [ ] Mint and verify the DOI-capable archival record; do not fabricate a DOI before the archive issues it.
- [ ] Verify clean-room reproduction from the frozen artifact.
- [ ] Test the final archival/artifact link from a logged-out/incognito environment.

## G. Venue-specific finalization

Do this only after the scientific package is frozen:

- [x] PVLDB Volume 20 / 2027 official-template build workspace exists.
- [x] Scientific source of truth is the canonical `paper/manuscript.md`.
- [x] Automated PVLDB build/page-gate workflow exists.
- [ ] Select the exact PVLDB submission round/date.
- [ ] Complete all author affiliations/coauthor metadata and ORCIDs as applicable.
- [ ] Verify current PVLDB supplemental-material/artifact rules immediately before submission.
- [ ] Verify current AI-use/disclosure policy immediately before submission.
- [ ] Verify COIs/reviewer nomination requirements if applicable.
- [ ] Final PDF visual inspection after all synchronized evidence is rendered.
- [ ] Final bibliography/citation audit.

## Stop condition

Do **not** reopen broad engineering development simply because the paper is in polishing stage. New experiments or code changes should be justified by a concrete reviewer risk, missing semantic baseline, correctness issue, or venue requirement. The current selector must not be retuned after the frozen held-out result merely to improve the reported `CollegeMsg` numbers.
