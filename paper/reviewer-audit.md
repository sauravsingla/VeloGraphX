# VeloGraphX reviewer-style submission audit

This is an internal pre-submission review of **VeloGraphX: Adaptive Exact Analytics for Evolving Graphs**. It is intentionally stricter than the repository README and is meant to surface plausible reject reasons before a conference reviewer does.

## Overall assessment

**Current paper potential:** strong top-tier database/systems submission if the manuscript stays narrow and evidence-backed.

| Dimension | Current assessment | Main reason |
| --- | --- | --- |
| Problem importance | Strong | Exact analytics on evolving graphs face a real repair-vs-recompute crossover. |
| Systems novelty | Strong but positioning-sensitive | The novelty is the integrated pre-repair exact execution decision over one mutable substrate, not the generic idea of incremental/static switching. |
| Technical depth | Strong | Mutable storage, exact maintained algorithms, pre-repair policy, correctness/fallback contracts, and reproducible evaluation are all implemented. |
| Experimental credibility | Strong | Pinned data/revisions, exactness gates, raw repetitions, current-selector oracle metrics, negative-result retention, and scoped external baselines. |
| Reproducibility | Very strong | Reviewer-facing artifact map, machine-readable evidence registry, retained run/artifact identifiers, reproduction workflow, and deterministic figure inputs. |
| Breadth of external dynamic baselines | Moderate-to-strong | NetworKit and RisGraph are useful, but the accepted campaigns are separate; do not fabricate a three-system league table from them. |
| Hardware breadth | Limited by design | Hosted evidence is adequate for the central relative/crossover claim but not for many-core/NUMA/microarchitecture claims. |
| Manuscript readiness | Strong draft | Main remaining work is venue formatting, integrated citations, figure polish, and final external review. |

## Likely reviewer objections and required responses

### 1. “Incremental-vs-static switching already exists.”

**Severity:** high if framed poorly; low if framed correctly.

GraphIn already uses dual incremental/static execution, and Bok et al. already use historical cost information to choose incremental or static processing. VeloGraphX must therefore avoid first-of-kind language about switching or cost-based selection.

**Required paper response:** define the contribution as the combination of:

- exact localized repair and full recomputation as two physical plans under one mutable graph substrate;
- a decision made *before* expensive repair discovery when possible;
- explicit prevention/measurement of repair-then-full double work;
- exact per-batch oracle/regret evaluation including wrong-arm and tail behavior; and
- an artifact-backed benchmark discipline that preserves competitor and selector losses.

**Status:** addressed in `related-work-notes.md`; must remain explicit in the final Related Work section.

### 2. “The selector is not uniformly near-oracle.”

**Severity:** medium.

The largest evaluated `web-Google` regime has a visible tail. This is a result, not an error to hide.

**Required paper response:** report both aggregate quality and the tail in the primary selector figure/table. Explain that the system claim is useful adaptive execution, not oracle optimality.

**Status:** addressed. Do not retune after seeing this tail unless a new development/holdout protocol is declared.

### 3. “Hosted runners are noisy.”

**Severity:** medium if making absolute peak-performance claims; low for the current claim scope.

**Required paper response:** emphasize same-run/paired comparisons, exact timing envelopes, repeated samples, dimensionless ratios where possible, and the explicit exclusion of many-core/NUMA/hardware-counter claims.

**Status:** addressed in the evidence registry and manuscript limitations.

### 4. “Why only BFS for the adaptive policy?”

**Severity:** medium.

**Required paper response:** explain that BFS is the controlled vehicle for studying repair/recompute selection because it exposes insertion and deletion dependency changes and admits a direct exact recomputation oracle. Use triangles, SSSP, CC, k-core, PageRank-related workflows, and storage results as system breadth evidence without pretending one selector transfers unchanged to every analytic.

**Status:** addressed, but the final introduction and limitations should state this plainly.

### 5. “The mutable storage design overlaps prior dynamic graph stores.”

**Severity:** medium.

GraphOne, Teseo, and related dynamic graph stores establish strong prior art for mutable graph representations and concurrent/evolving-graph storage.

**Required paper response:** do not claim the storage layout itself is the sole novelty. Position segmented CSR + packed deltas + sparse row patches as the substrate that enables the paper's exact execution-choice study and separately report its bounded canonicalization trade-off.

**Status:** bibliography/related-work coverage should explicitly include GraphOne and Teseo.

### 6. “External baseline coverage is fragmented.”

**Severity:** medium.

NetworKit and RisGraph accepted evidence comes from separate hosted campaigns. Absolute times from those runs must not be combined.

**Required paper response:** report each scoped comparison separately and preserve the fact that competitors win some workloads. A future audited unified same-run campaign would strengthen the paper but is not a prerequisite for the current central claim.

**Status:** evidence rules already enforce this.

### 7. “The paper has too many secondary stories.”

**Severity:** high if not controlled.

The repository contains storage, compression, multicore, Python packaging, multiple algorithms, and many benchmark campaigns. A paper that gives them equal weight will look like a project report rather than a focused systems contribution.

**Required paper response:** keep the hierarchy:

1. repair/recompute crossover;
2. current pre-repair selector;
3. external dynamic/static context;
4. triangle breadth;
5. large-graph storage mechanism;
6. everything else in supporting/appendix material.

**Status:** current manuscript follows this hierarchy; preserve it.

## Simulated scores

These are internal readiness scores, not acceptance probabilities.

| Review category | Score / 10 |
| --- | ---: |
| Significance | 9.0 |
| Technical quality | 9.3 |
| Novelty after proper positioning | 8.7 |
| Evaluation | 9.2 |
| Reproducibility | 9.8 |
| Clarity potential | 9.1 |
| Artifact quality | 9.7 |
| Overall submission readiness | 9.2 |

## Must close before submission

1. Convert the internal “novelty boundary” wording into a polished Related Work section with real citations.
2. Include GraphOne and Teseo in mutable-storage positioning.
3. Ensure every headline number in the abstract appears in a figure/table or has an obvious evidence pointer.
4. Generate and visually inspect all final figures at two-column print size.
5. Add dataset/workload and hardware/software environment tables.
6. Freeze the exact submission commit and raw selected evidence in a durable archival release; do not rely only on expiring Actions artifacts.
7. Perform one independent database-systems review focused on novelty and missing baselines.
8. Apply the chosen venue's anonymity, page-limit, artifact, COI, and AI-disclosure rules only after the scientific content is frozen.

## Explicit non-blockers

The following are **not required** for this paper unless the claim scope changes:

- buying dedicated hardware;
- 32/64-core scalability;
- multi-socket NUMA experiments;
- AVX-512 superiority;
- adding more graph algorithms;
- retuning the selector to eliminate every tail;
- producing a universal fastest-system league table.

## Submission gate

The paper is ready to enter venue-formatting and external-review stage when all items under **Must close before submission** are either completed or deliberately documented as venue-specific finalization tasks. Core engine feature development should remain frozen during this stage.
