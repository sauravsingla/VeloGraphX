# BFS selector v3 H6 preregistration

Status: **frozen before any H6 selector measurement is observed**.

This document freezes the first unseen evaluation of the reusable bounded-cascade candidate `publication-preflight-bfs-v3-cascade-reuse2`. The road-smallworld and chain-hubs-directed workloads used during development are now observed development evidence only and must not be described as unseen for this candidate.

## Frozen candidate

Development base on `main`: `d6ed5b02e910864efaab41ba338b87fb9a414585`.

Candidate development commit with unchanged road/H4 gates passing: `fd1f0bf6139e4ecdf418d6a46fce0d81a35809f6`.

Selector identifier: `publication-preflight-bfs-v3-cascade-reuse2`.

The candidate is generated from the following frozen Git blobs. H6 execution must verify these identities before compiling:

- `benchmarks/publication_policy_bfs_v3_light3.cpp`: `c2243113f43d12f4f0baeba547418caad4642f42`
- `include/velographx/incremental/bfs.hpp`: `b726a6ef97733218d834c0106d6509f9714bebc1`
- `tools/prepare_bfs_reusable_preflight_header.py`: `da322a06e73f979d45dceb93bfade016196f6ba0`
- `tools/prepare_bfs_selector_v3_candidate.py`: `bddfe973208a16183b0c156b91164688ee109d17`
- `tools/prepare_bfs_selector_v3_reuse_candidate.py`: `19736945ae5791b798988916ae95437b14f7bfc7`
- `tools/prepare_bfs_selector_v3_reuse2_candidate.py`: `cd51c4358bdfe531ba735bd640b3195d495eb454`
- `tools/summarize_bfs_selector_runs.py`: `484c263f55bcb3c0fb854cd372bb6df5a6e9ad47`

The H6 generator was committed before this preregistration and before any H6 benchmark run:

- generator: `tools/generate_bfs_selector_v3_h6.py`
- generator commit: `a5c2aeaa5ae25868efaef017c2167351d18c3997`
- generator Git blob: `b6c57101595648bb338a770bd85ae0e13c5a3e13`
- generator version: `bfs-selector-v3-h6-v1`

No selector source, threshold, reusable-preflight implementation, summarizer semantics, or H6 generator may change after H6 results are observed while retaining the H6 holdout label. Any result-informed change requires a new preregistered holdout (H7 or later).

## Development evidence available before H6

GitHub Actions run `34871616101`, artifact `10358767329`, artifact ZIP SHA256 `cbbc4f8de53c1ddbeacbb583ae5aa0ee85767001df45eaafd356e9c371f22ddc`, evaluated only already-seen road-smallworld and H4 chain-hubs-directed data.

The run preserved 100% exactness and zero internal repair-to-full fallback. Aggregate adaptive-selector development results were:

- road-smallworld: mean regret `0.04980106988064926`, p95 `0.2598399295722971`, p99 `0.5446064200957126`, maximum `1.5779226651976948`, wrong-arm rate `0.010973936899862825`;
- H4 chain-hubs-directed: mean regret `0.0864548583807878`, p95 `0.45668109180824323`, p99 `0.5936393947855274`, maximum `0.7858972647189421`, wrong-arm rate `0.0990990990990991`.

The reusable cascade mechanism was exercised: 1,041 previews evaluated, 75 exceeded the bound and selected full recomputation, and 966 prepared previews were reused by incremental execution.

These numbers are development evidence only and are recorded here so the H6 acceptance rule cannot be adjusted after seeing H6.

## Fresh holdout H6 — BFS

Graph family: `layered-diamond-directed`.

This family was not used to design or tune `cascade-reuse2`. It is structurally different from the observed road/grid and chain-hub families: directed cyclic layers, overlapping forward diamonds, sparse reverse-local/inter-layer edges, a bidirectional anchor spine, and deterministic long inter-layer alternatives.

Frozen graph parameters:

- generator version: `bfs-selector-v3-h6-v1`
- seed: `2026091406`
- vertices: `49152`
- layer width: `256`
- layers: `192`
- expected edges: `164336`
- expected graph SHA256: `f97dbd6c0fa63ee608a063ab608e11276435ce5e4e1bd85c8ba2af713ba5b7d1`
- roots: `0,24661,49151`
- import rate: `0.95`
- batch sizes: `128,512,2048`
- repetitions: `3`
- simple-threshold update fraction: `0.01`
- hosted runner: Ubuntu 22.04
- OpenMP threads: `1`

The workflow must fail before measurement if generator metadata or graph SHA256 differs from the values above.

## Frozen evaluation metrics

For the adaptive selector, report without suppressing unfavorable regimes:

- exactness;
- mean / median / p95 / p99 / maximum same-run oracle regret;
- wrong-arm rate;
- full-choice fraction;
- selector decision cost;
- internal repair-to-full fallback fraction;
- reusable-preflight evaluated / exceeded / reused counts;
- per-root and per-batch regime metrics.

Oracle regret remains `R = (T_selected - T_oracle) / T_oracle`, with `T_oracle = min(T_incremental, T_full)` from the same run.

## Frozen hosted generalization gates

These are the unchanged H4-style development/generalization gates already in use before H6. They are **not publication-grade performance claims**:

- exactness = 100%;
- internal fallback fraction = 0;
- mean oracle regret <= 10%;
- p95 oracle regret <= 50%;
- wrong-arm rate <= 12%;
- reusable cascade mechanism must be exercised (`preview_evaluated > 0`, `preflight_reused > 0`).

Maximum and p99 regret are mandatory reporting metrics but are not hard-fail thresholds on hosted CI because same-arm timing variation can create tail regret without a wrong-arm decision. Any tail outlier must still be retained and discussed.

If any frozen gate fails, H6 is a failed holdout and the unfavorable result is retained. The selector may then be redesigned only by converting H6 to development evidence and preregistering a new H7 holdout before another generalization claim.

## Claim boundary

Every H6 artifact must contain `research_claim=false`. Hosted CI establishes correctness, execution-contract behavior and development/generalization evidence only. Dedicated controlled hardware, publication-grade same-machine external baselines, hardware/NUMA evidence where claimed, and independent reproduction remain separate publication requirements.
