# VeloGraphX submission checklist

This checklist freezes the transition from engineering/research development to a conference submission package.

## A. Scientific scope freeze

- [x] Central thesis fixed: exact localized repair and full recomputation are competing execution strategies whose preferred choice changes by graph/update regime.
- [x] Current publication selector identified and audited.
- [x] Negative selector tail retained rather than tuned away.
- [x] External competitor wins retained.
- [x] Dedicated hardware excluded as a requirement for the core claim.
- [x] Many-core/NUMA/NVMe/microarchitecture claims excluded from manuscript scope.
- [ ] No new core feature is merged without a paper-critical justification.

## B. Novelty and related work

- [x] GraphIn recognized as prior dual incremental/static execution.
- [x] Bok et al. recognized as prior history/cost-based incremental/static selection.
- [x] GraphBolt and DZiG recognized as dependency/sparsity-aware incremental processing.
- [x] RisGraph recognized as prior low-latency evolving-graph analytics.
- [x] Layph recognized as prior work on constraining broad change propagation.
- [ ] GraphOne included in final mutable-storage positioning.
- [ ] Teseo included in final mutable-storage positioning.
- [ ] Final manuscript uses citation-backed academic prose rather than internal “first/not first” notes.
- [ ] One independent DB-systems researcher reads the novelty/related-work sections before submission.

## C. Evidence and statistics

- [x] Current selector evidence has retained run/artifact IDs and digest.
- [x] Current selector exactness, mean regret, weighted regret, wrong-arm rate, decision overhead, and tail are recorded.
- [x] NetworKit paired campaign retained with exactness.
- [x] GAP/LAGraph static context retained.
- [x] RisGraph comparison retained separately.
- [x] Published exact triangle reference comparison retained.
- [x] Orkut canonicalization A/B retained.
- [x] Multi-dataset triangle crossover retained as supporting evidence.
- [x] Cross-run absolute timing league tables prohibited.
- [ ] Final figures show repetitions/dispersion where supported.
- [ ] Dataset/workload table created.
- [ ] Hardware/software/environment table created.
- [ ] Every abstract number maps visibly to a figure/table or evidence row.

## D. Figures and tables

Required final paper surfaces:

- [ ] **Figure 1:** system architecture / execution path.
- [ ] **Figure 2:** current selector regret/crossover across graph/regime.
- [ ] **Table 1:** datasets, graph family, vertices/edges, update regime, roots/repetitions.
- [ ] **Table 2:** external baseline summary with workload-specific wins/losses.
- [ ] **Figure/Table 3:** exact dynamic triangle published-reference result.
- [ ] **Figure/Table 4:** Orkut canonicalization A/B memory/performance trade-off.
- [ ] Optional supporting figure: multi-dataset triangle crossover.
- [ ] Optional supporting table: 1/2/4-thread static context.
- [ ] All figures readable at final conference column width and in grayscale/print.
- [ ] Captions state timing boundary, repetitions, and exactness where relevant.

## E. Manuscript quality

- [ ] Abstract tightened to venue-appropriate length while retaining exactness, central selector result, one external-context result, and scoped conclusion.
- [ ] Introduction ends with a crisp problem gap and four contributions.
- [ ] Related Work is integrated once, without duplicated “prior art” and “related work” sections.
- [ ] System design explains the pre-repair decision point and repair-then-full double-work risk visually.
- [ ] Experimental methodology states hosted-runner scope before results.
- [ ] Evaluation follows the central-story hierarchy rather than repository feature breadth.
- [ ] Limitations explicitly include the historical-root validation and large-web tail.
- [ ] Conclusion states a systems lesson, not a universal fastest-system claim.
- [ ] All terminology is consistent: `localized repair`, `full recomputation`, `pre-repair selector`, `oracle regret`, `wrong-arm`.

## F. Artifact and archival package

- [x] Reviewer-facing `PAPER.md` exists.
- [x] Minimal reproduction command exists.
- [x] Evidence registry exists in human-readable and JSON form.
- [x] Current selector manual reproduction workflow exists.
- [x] Deterministic figure generator reads only committed paper evidence.
- [ ] Freeze selected raw artifacts outside GitHub Actions retention.
- [ ] Create a submission tag/release from the exact submitted commit.
- [ ] Archive the submission artifact in a DOI-capable repository when appropriate.
- [ ] Verify clean-room reproduction from the frozen artifact.
- [ ] Prepare anonymous artifact snapshot for any double-anonymous venue.

## G. Venue-specific finalization

Do this only after the scientific package is frozen:

- [ ] choose target venue/round;
- [ ] apply official template and page limit;
- [ ] verify anonymity model;
- [ ] verify artifact/supplemental-material rules;
- [ ] verify author profiles, ORCIDs, COIs, reviewer nomination requirements if any;
- [ ] verify AI-use/disclosure policy if applicable;
- [ ] verify simultaneous-submission and prior-publication rules;
- [ ] final PDF visual inspection;
- [ ] final artifact-link test from a logged-out/incognito environment.

## Stop condition

Do **not** reopen broad engineering development simply because the paper is in polishing stage. New experiments or code changes should be justified by a concrete reviewer risk, missing semantic baseline, correctness issue, or venue requirement.
