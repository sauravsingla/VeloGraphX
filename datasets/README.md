# Datasets

Do not commit large third-party datasets to the repository. Benchmark scripts should download or generate datasets reproducibly and record licenses, hashes, seeds and preprocessing parameters.

The publication benchmark contract is [`publication-benchmark-contract.json`](publication-benchmark-contract.json). It prevents dataset selection from drifting toward only favorable VeloGraphX workloads.

## Required graph families

A publication-level campaign must cover all of the following families:

| Family | Canonical examples | Purpose |
| --- | --- | --- |
| Scale-free web | `web-Google` | Skewed web topology used broadly in graph-systems evaluation |
| Scale-free social/community | `soc-Epinions1`, `com-LiveJournal`, `com-Orkut` | Multiple skewed real-world graph scales |
| Road network | `roadNet-CA` | Low-degree, high-diameter counterpoint to scale-free graphs |
| Synthetic Kronecker/R-MAT | Graph500-style generator | Controlled scalability and memory-capacity study |

The canonical road workload is SNAP `roadNet-CA`: 1,965,206 vertices and 2,766,607 undirected edges in the source dataset. Downloaded bytes must be checksum-pinned by the benchmark campaign before results are accepted.

The complete controlled-hardware manifest is [`manifest.publication-canonical.json`](manifest.publication-canonical.json). It pins `web-Google`, `com-LiveJournal` and `roadNet-CA` by SHA-256 and declares the exact observed vertex/row counts enforced during preparation. The unified runner is documented in [`docs/canonical-publication-campaign.md`](../docs/canonical-publication-campaign.md).

## Synthetic graph parameters

The default Kronecker/R-MAT contract uses:

- `A=0.57`, `B=0.19`, `C=0.19`, `D=0.05`
- edge factor `16`
- scales `16, 18, 20, 22, 24, 26`
- a fixed seed recorded in every artifact

A campaign may use additional scales, but it must report the exact generator implementation, scale, edge factor, initiator probabilities and seed. Generated graphs are supplementary scalability workloads, not replacements for real public datasets.

## Memory-limit campaign

On dedicated benchmark hardware, attempt successively larger configured graphs and report the largest graph that completes without swapping or OOM. Retain machine RAM, NUMA topology, peak RSS and the next attempted scale when available. Out-of-memory/external-memory experiments must be labelled separately from the in-memory campaign.
