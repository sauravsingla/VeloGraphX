# Submission archival status

VeloGraphX keeps software releases and paper-artifact freezes separate.

- **Software release:** `v0.8.2`
- **Current paper freeze contract:** `pvldb-2027-submission-v4`
- **DOI provider:** Zenodo
- **DOI status:** pending external authorization; no DOI is claimed until one is actually minted

## What the paper freeze preserves

The freeze workflow creates an immutable annotated tag and GitHub release for the exact merged commit. It attaches:

1. the deterministic source archive;
2. SHA-256 checksums;
3. an evidence manifest;
4. a persistent evidence bundle containing the selected raw GitHub Actions artifacts used by the central paper results; and
5. either the real Zenodo publication result or an explicit status file saying that Zenodo authorization is not configured.

The selected retained raw artifacts are:

| Evidence | Run | Artifact | Recorded SHA-256 |
| --- | ---: | ---: | --- |
| Primary selector | 34929398888 | 10381490811 | `a78a421663b08228a7bd26596f9ad0498e2ad4a0c1470c79471dcaef97885a7b` |
| Production fallback | 35237513376 | 10503926632 | `26c37720fef6524ee68e816a095f7dd591c5cfd93c2f112a8de7ebd8874a621a` |
| Frozen held-out selector | 35237513595 | 10504131673 | `3cc4394693818450523fc56a9bb4368f7e26365d19077f2835a915a2bc312452` |
| Clean selector ablation | 35237513377 | 10504591544 | `a0aa985cd3f64d2e340938ba0111ec017076e24412665213bd4a9f4d5bf3601d` |
| Matched GraphBolt comparison | 35237513587 | 10503851688 | `6d3a756040276aa4c49f4820bf31d3d0b60025affcc024cd9bb04cb2eddc9b59` |

The freeze workflow downloads each artifact while it is still available from Actions, verifies the recorded digest, and stores the bundle as a release asset so these core results are no longer dependent on Actions-retention lifetime.

## Public-access and clean-room gate

After creating the release, the workflow downloads the source archive through the public GitHub release URL **without an Authorization header**, verifies its checksum, extracts it to a new directory, and runs `scripts/reproduce_minimal.sh` with build/artifact directories outside the extracted source tree.

A successful freeze therefore verifies both public accessibility and a minimal clean-source reproduction.

## Latest-release behavior

Paper freezes are archival snapshots, not product releases. The freeze workflow marks the paper release as not-latest and restores the configured software release (`v0.8.2`) as GitHub's latest release. README badges likewise link separately to software and paper artifacts.

## DOI boundary

The repository cannot mint a DOI without external Zenodo authorization. When `ZENODO_ACCESS_TOKEN` (or a compatible GitHub-Zenodo integration) is configured, the same freeze workflow uploads the evidence-bearing archival bundle to Zenodo and records the returned DOI.

Until then:

- `CITATION.cff` intentionally contains no DOI;
- the GitHub tag/release/checksums remain valid;
- the DOI checklist item remains open; and
- no placeholder or guessed DOI should be published.
