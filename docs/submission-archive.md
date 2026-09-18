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
| NetworKit comparison | 33542995289 | 9814639042 | `acf743bcac2542660fa050d70105e5e1e5f79d9e22ef1ec7324cf1e147ae5f12` |
| GAP/LAGraph static comparison | 33418520303 | 9768499895 | `a8580906f1b6d1ac431f2bdf969a4a00f00d5ff4bf31f554b5e8731606e1c34c` |
| RisGraph comparison | 33286241439 | 9724535579 | `ce0ad5ba22f0190ea6fe5f55226364f6749ed9844fb6c01f5ce494c604123de2` |
| GoldenCounter exact triangles | 33248107299 | 9713495404 | `f032b9382130eac154c40c7f1f54af07363a2a9e1bc4190beb3d0af0ad830021` |
| Orkut canonicalization A/B | 33265264254 | 9718634869 | `f3d17c882d26a1f5561e891c8d430bf11289e1be02cc03bdc0cab64be6f6e942` |
| Triangle crossover: p2p-Gnutella08 | 34927599394 | 10381010037 | `171727df8327519da4d45d4827df855c93b63131df12a1d5e001bc7e7dac54ae` |
| Triangle crossover: facebook-combined | 34927599394 | 10380526227 | `6c31bd3032123d91c400715fe598ec820f93290a0b0673d9f94947a13014b3d6` |
| Triangle crossover: ca-HepTh | 34927599394 | 10379783827 | `f710ca8e08b6b90f25784a7868c2d33ee116608f6439da7e2b19fd33344175f9` |

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
