# Post-PR71 reviewer-closure preregistration

This record was added **after** `publication-preflight-v1` was frozen in PR #71 and **before** observing results from the campaign defined below. It exists to prevent an evaluation workload from silently becoming development data.

## Frozen selector under test

- Base `main` commit before this campaign: `559adc1c77f5deffcede9bc4aea1327bf53bc2a9`.
- BFS selector under test: `publication-preflight-v1` in `benchmarks/publication_policy_bfs.cpp`.
- Historical selector remains frozen separately as `bounded-one-sided-warmup-v5`.
- The motivating `web-Google` / root `481807` tail workload is development evidence and is **not** reused as held-out evidence here.

If the BFS selector thresholds, features, model structure, or decision rules are changed after observing this campaign, this campaign becomes development evidence and a new holdout must be preregistered.

## H1 — unseen graph-family generalization

Use the deterministic generator `tools/generate_reviewer_holdout.py` in `road-smallworld` mode:

- generator version: `reviewer-holdout-v1`;
- seed: `20260914`;
- dimensions: `192 x 192` (`36,864` vertices);
- graph structure: bidirectional 2-D road/grid backbone plus sparse long directed shortcuts;
- import rate: `0.95`;
- roots selected structurally, without timing: `0`, `18,528`, `36,863`;
- batch sizes: `128`, `512`, `2048` source edges (each sliding batch also removes the corresponding oldest edges);
- simple-threshold baseline: `0.01`;
- repetitions on hosted CI: `2` per root/batch regime.

The graph is a deliberately different low-degree/high-diameter family from the development graphs used for selector tuning. The generated edge-list SHA-256 is retained in every artifact.

Required reporting, regardless of outcome:

- exactness for every policy;
- adaptive mean/median/p95/p99/max oracle regret;
- wrong-arm rate;
- selector decision cost;
- full-recompute fraction;
- internal fallback fraction;
- per-root and per-batch regime results.

No hosted-runner performance threshold is allowed to delete or suppress an unfavorable regime. Hosted measurements remain `research_claim=false`.

## H2 — clean A0–A7 causal ablation

Run `benchmarks/publication_ablation_bfs.cpp` on the **same held-out stream**, one root and multiple batch regimes, with explicit stages:

| Stage | Mechanism |
| --- | --- |
| A0 | canonical CSR rebuild + full BFS |
| A1 | compact mutable storage + full BFS |
| A2 | localized exact repair |
| A3 | localized repair + affected-work fallback |
| A4 | + graph-scale/root-state preflight |
| A5 | + online cost prediction |
| A6 | + uncertainty-aware choice |
| A7 | + selector-owned recomputation |

This harness is the causal ablation. Historical selector-development runs remain motivation only and must not be relabeled as an A0–A7 experiment.

The A1/A2 pair defines the two-arm oracle for the ablation. A0 includes storage-rebuild cost and is therefore not used as an oracle arm.

## H3 — cross-algorithm transfer

Use `tools/generate_reviewer_holdout.py` in `clustered-undirected` mode and run `benchmarks/publication_policy_triangles.cpp`:

- `8,192` vertices;
- ring-lattice local structure with deterministic random chords;
- seed `20260914`;
- import rate `0.90`;
- batch sizes `64`, `256`, `1024`;
- policies: always incremental, always full, simple threshold, history-cost baseline, adaptive pre-repair policy, and offline two-arm oracle.

This experiment tests transfer of the **pre-repair two-arm/oracle-regret framework** to exact triangle counting. It does **not** claim that the BFS feature set transfers unchanged. Triangle policy signals are algorithm-appropriate and the result is labelled cross-algorithm engineering evidence until rerun on publication hardware.

## Publication boundary

The following remain impossible to establish on GitHub-hosted runners and are intentionally not claimed by this campaign:

- publication-grade absolute performance;
- stable many-core scaling;
- genuine multi-socket NUMA effects;
- hardware-counter conclusions;
- controlled same-machine external-system superiority;
- independent reproduction.

The canonical dedicated-hardware campaign remains the gate for those claims. This reviewer-closure campaign improves causal, held-out, and cross-algorithm evidence without weakening that boundary.
