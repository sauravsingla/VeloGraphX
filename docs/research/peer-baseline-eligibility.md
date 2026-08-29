# Peer-baseline eligibility and same-CPU evidence

This note records which recent/related systems can be compared with VeloGraphX under a strict **same data + same environment + same CPU** contract, and separates original-system reproduction from paper-derived reimplementation.

## Eligibility

| Work | Published execution model | Exact triangle counting? | Same-CPU original implementation eligible? | Reason |
| --- | --- | --- | --- | --- |
| Makkar, Bader, Green, **HiPC 2017**, _Exact and Parallel Triangle Counting in Dynamic Graphs_ | NVIDIA GPU + cuSTINGER | Yes | **No** | The published implementation/performance evaluation is GPU-based. The algorithm is hardware-independent, so VeloGraphX includes an independent CPU reimplementation of the paper's insertion inclusion-exclusion formulation for algorithm-level comparison only. |
| Wang et al., **IEEE TPDS 2026**, _EDTC: Exact Triangle Counting for Dynamic Graphs on GPU_ | GPU | Yes | **No** | EDTC is explicitly designed and evaluated as a GPU dynamic triangle-counting system. Running a CPU translation would not reproduce the published implementation. |
| **GraphDelta 2026**, Journal of Systems Architecture | Spark/GraphX distributed framework | No triangle benchmark in the paper; PageRank, Connected Components and SSSP | **No for the triangle campaign** | It is not an exact dynamic triangle-counting baseline, and no public source repository was identified during the reproducibility search. |

No performance number from an ineligible row should be described as a same-CPU system-vs-system comparison.

## HiPC 2017 algorithm-derived CPU experiment

VeloGraphX contains `benchmarks/hipc2017_cpu_reimplementation.cpp`, an independent CPU implementation of the **insertion portion of Algorithm 2** from HiPC 2017. It reproduces the paper's three intersection phases and inclusion-exclusion correction for insertion batches using sorted CPU adjacency lists.

This is **not the authors' cuSTINGER/GPU implementation** and therefore is not evidence that either system is faster than the published HiPC implementation. It is an algorithm-level CPU comparison under a normalized execution contract.

### Contract

- GitHub-hosted Ubuntu runner
- same C++ compiler invocation and optimization level
- same normalized `facebook-combined` graph: 4,039 vertices / 88,234 undirected edges
- identical deterministic missing-edge insertion sequence for both algorithms
- identical 1%, 5% and 10% batch sizes
- five repetitions per fraction
- VeloGraphX timing includes graph mutation + exact incremental maintenance
- HiPC-derived answer-ready timing includes sorted-adjacency graph mutation + the three inclusion-exclusion intersection phases
- every result checked against VeloGraphX full recomputation
- `research_claim: false`; `publication_grade: false`

### Median results

| Update batch | VeloGraphX exact update | HiPC-2017-derived CPU answer-ready | HiPC-derived / VeloGraphX time | VeloGraphX throughput | HiPC-derived throughput |
| --- | ---: | ---: | ---: | ---: | ---: |
| **1% / 883 edges** | 1.153 ms | **0.425 ms** | **0.368x** | 0.766M updates/s | **2.079M updates/s** |
| **5% / 4,412 edges** | 7.164 ms | **2.243 ms** | **0.313x** | 0.616M updates/s | **1.967M updates/s** |
| **10% / 8,824 edges** | 16.503 ms | **5.289 ms** | **0.320x** | 0.535M updates/s | **1.668M updates/s** |

Equivalently, the HiPC-derived CPU implementation is approximately **2.72x**, **3.19x**, and **3.12x faster** than the current VeloGraphX triangle update path at 1%, 5%, and 10% respectively under this hosted-CI experiment.

All **15/15** measured outputs matched exact full recomputation.

## Interpretation

This result is useful precisely because it is not selected to make VeloGraphX look better. It shows that VeloGraphX's current per-update exact triangle maintenance is correct and substantially faster than its own full recomputation, but a batch-oriented inclusion-exclusion formulation can perform materially less CPU work on the same insertion batches.

The appropriate next engineering experiment is therefore an **ablation between VeloGraphX's sequential per-edge maintenance and a batch-aware exact insertion path**, while preserving deletion/mixed-update correctness and the engine's adaptive execution contract. Any future VeloGraphX improvement derived from the HiPC formulation must be presented as engineering adoption of known algorithmic ideas rather than new triangle-counting novelty.

## Sources

- Devavret Makkar, David A. Bader, Oded Green, _Exact and Parallel Triangle Counting in Dynamic Graphs_, IEEE HiPC 2017, DOI `10.1109/HiPC.2017.00011`.
- Z. Wang et al., _EDTC: Exact Triangle Counting for Dynamic Graphs on GPU_, IEEE Transactions on Parallel and Distributed Systems, 2026, DOI `10.1109/TPDS.2025.3627974`.
- _GraphDelta: A distributed incremental framework for efficient dynamic graph computing in edge intelligence_, Journal of Systems Architecture, Vol. 176, 2026, Article 103834.
