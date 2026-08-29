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
| Makkar, Bader, Green, HiPC 2017, *Exact and Parallel Triangle Counting in Dynamic Graphs* | exact dynamic triangle counting; published implementation on cuSTINGER/Hornet GPU stack | **Not eligible on CPU-only hosted runner** | The paper states its implementation and performance analysis are on GPU. The open implementation is tied to the GPU dynamic-graph stack. Although the algorithm can in principle be implemented for CPU, a VeloGraphX-authored CPU port would be a new implementation, not the published system. |
| Wang et al., IEEE TPDS 2026, *EDTC: Exact Triangle Counting for Dynamic Graphs on GPU* | exact dynamic triangle counting on GPU | **Not eligible on CPU-only hosted runner** | EDTC is explicitly a GPU system. No CPU implementation is used by the published system, so a same-CPU execution would require a new port and would not constitute reproduction of EDTC. |
| He et al., Journal of Systems Architecture 2026, *GraphDelta* | distributed incremental PageRank, Connected Components and SSSP on Spark/GraphX | **Not eligible for triangle-count table** | GraphDelta evaluates different algorithms and a distributed Spark/GraphX execution model. Comparing it to VeloGraphX triangle maintenance would change both workload semantics and runtime architecture. |
| SIGMOD 2021 public triangle-counting source `GoldenCounter` exact reference | exact global triangle count reference, CPU-compatible | **Eligible and executed** | Public pinned source can run on the same CPU runner, consume the identical normalized graph and deterministic insertion batches, and produce an exact triangle count for cross-checking. |

## Executed direct comparison

The eligible exact reference comparison is implemented by `.github/workflows/same-run-published-baseline.yml` and documented in [`same-run-published-baseline.md`](same-run-published-baseline.md).

On `facebook-combined`, five repetitions per update fraction produced exact agreement across VeloGraphX incremental maintenance, the published exact reference and VeloGraphX full recomputation. Median exact-answer-ready latency ratios were:

- 1% / 883 insertions: **40.95x lower latency** for VeloGraphX;
- 5% / 4,412 insertions: **6.94x lower latency**;
- 10% / 8,824 insertions: **3.48x lower latency**.

These figures apply only to the pinned exact reference component and do not imply superiority over the associated paper's approximate algorithm.

## What would make HiPC 2017 or EDTC directly comparable

A direct comparison requires GPU hardware and the original/pinned GPU implementation, with the same graph, update stream, update-batch semantics, correctness contract and measurement boundary. Running VeloGraphX on CPU and the published systems on GPU can be useful as a heterogeneous systems study, but it is not a same-CPU comparison and must be labeled accordingly.

For a genuine CPU algorithmic study, an independently specified CPU implementation of the HiPC inclusion-exclusion method could be developed and benchmarked under the same harness. Such a result should be labeled **"CPU reimplementation of the published algorithm"**, not **"published implementation"**.

## Interpretation

The absence of a direct HiPC/EDTC/GraphDelta row is deliberate, not missing evidence. The benchmark contract prevents hardware-incompatible or workload-incompatible systems from being forced into a table that would look quantitative but would not be scientifically defensible.
