# Published baseline eligibility for same-CPU benchmarking

This note records which published dynamic-graph systems can be included in a strict same-data, same-environment, same-CPU benchmark against VeloGraphX.

## Eligibility rule

A result is eligible for a direct performance table only when all of the following hold:

1. public runnable implementation is available;
2. implementation can execute on the same CPU-only GitHub-hosted Linux runner;
3. workload semantics match the VeloGraphX workload being measured;
4. identical normalized dataset and identical deterministic update sequence can be supplied;
5. exact correctness can be checked against a trusted recomputation result;
6. compiler/runtime and measurement boundaries can be captured and reported.

A CPU reimplementation written by VeloGraphX authors from a paper description is **not** treated as the published implementation and therefore is not used for a direct published-system speed claim.

## Candidate review

| Published work | Primary workload/platform | Same-CPU direct benchmark status | Reason |
| --- | --- | --- | --- |
| Makkar, Bader, Green, HiPC 2017, *Exact and Parallel Triangle Counting in Dynamic Graphs* | exact dynamic triangle counting; published implementation on cuSTINGER/Hornet GPU stack | **Not eligible on CPU-only hosted runner** | The published implementation/performance analysis are GPU-based. A VeloGraphX-authored CPU port would be a new implementation, not reproduction of the published system. |
| Xuan et al., SRDS 2024, *DTC: Real-Time and Accurate Distributed Triangle Counting in Fully Dynamic Graph Streams* | distributed **approximate** fully dynamic triangle counting; multi-machine CPU runtime; public code | **Not eligible for exact same-semantics table** | Strong recent conference baseline with open code, but DTC estimates triangle counts rather than maintaining the exact global count. Reusing the same Facebook stream would still compare different correctness contracts. |
| Xia, Fang, Luo, SIGMOD 2025, *Efficiently Counting Triangles in Large Temporal Graphs* | exact CPU temporal-triangle queries over timestamped graphs | **Not eligible for dynamic-update table** | Top-conference CPU exact triangle work, but the problem is δ-temporal triangle counting in query windows, not exact maintenance after edge insert/delete updates. No public implementation tied to the paper was found during the reproducibility screen. |
| Zhang et al., TACO 2025, *Cheetah: Accelerating Dynamic Graph Mining with Grouping Updates* | CPU dynamic graph-pattern mining including triangle counting | **Promising research context; not eligible as a reproduced published baseline** | Cheetah is semantically closer and evaluates triangle counting on dynamic graphs, but it is a journal article rather than the requested top conference and no public runnable source was found in the reproducibility screen. |
| Wang et al., IEEE TPDS 2026, *EDTC: Exact Triangle Counting for Dynamic Graphs on GPU* | exact dynamic triangle counting on GPU | **Not eligible on CPU-only hosted runner** | EDTC is explicitly a GPU system. Same-CPU execution would require a new port and would not reproduce EDTC. |
| He et al., Journal of Systems Architecture 2026, *GraphDelta* | distributed incremental PageRank, Connected Components and SSSP on Spark/GraphX | **Not eligible for triangle-count table** | Different algorithms and a distributed Spark/GraphX execution model. |
| SIGMOD 2021 public triangle-counting source `GoldenCounter` exact reference | exact global triangle-count reference, CPU-compatible | **Eligible and executed** | Public pinned source runs on the same CPU runner, consumes the identical normalized graph and deterministic insertion batches, and returns an exact count for cross-checking. |

## Post-2023 top-conference screen

The strongest recent conference hits were SRDS 2024 DTC and SIGMOD 2025 temporal triangle counting. Neither satisfies the full VeloGraphX direct-comparison contract:

- **SRDS 2024 DTC** has public code and CPU execution, but its output is approximate and distributed, so an exact result-vs-result latency table would be semantically invalid.
- **SIGMOD 2025 temporal triangle counting** is exact and CPU-oriented, but it answers temporal-window queries on timestamped graphs rather than maintaining the exact ordinary triangle count after graph updates.

Therefore no post-2023 top-conference paper found in this screen is both **CPU-runnable + exact + dynamically update-equivalent + openly reproducible** under the current hosted runner.

## Executed direct comparison

The eligible exact reference comparison is implemented by `.github/workflows/same-run-published-baseline.yml` and documented in [`same-run-published-baseline.md`](same-run-published-baseline.md).

On `facebook-combined`, five repetitions per update fraction produced exact agreement across VeloGraphX incremental maintenance, the published exact reference and VeloGraphX full recomputation. Median exact-answer-ready latency ratios were:

- 1% / 883 insertions: **40.95x lower latency** for VeloGraphX;
- 5% / 4,412 insertions: **6.94x lower latency**;
- 10% / 8,824 insertions: **3.48x lower latency**.

These figures apply only to the pinned exact reference component and do not imply superiority over the associated paper's approximate algorithm.

## What would make a recent paper directly comparable

A recent candidate becomes eligible only when its original implementation can be pinned and executed under the same CPU runner while consuming the identical normalized graph/update stream and producing the same exact triangle-count semantics. CPU ports, semantic adapters that change the target metric, or approximate-vs-exact comparisons must be reported separately and cannot enter the headline same-run table.

## Interpretation

The absence of a post-2023 top-conference row is deliberate, not missing evidence. The benchmark contract prevents hardware-incompatible, approximate, or workload-incompatible systems from being forced into a table that would look quantitative but would not be scientifically defensible.
