# VeloGraphX Novelty Ledger

This ledger prevents accidental novelty claims. Status values are: **known prior art**, **integration hypothesis**, **candidate novelty**, **validated contribution**, or **rejected**.

| Idea | Closest prior-art theme | VeloGraphX difference to test | Hypothesis / experiment | Status |
|---|---|---|---|---|
| CSR + mutable delta adjacency | Dynamic graph stores using base + update buffers/logs | Degree- and workload-aware representation plus measured compaction cost | Compare traversal/update/memory across update rates | known prior art / integration hypothesis |
| Adaptive compaction | LSM/log/delta graph stores and packed layouts | Cost model uses measured query penalty, mutation rate, fragmentation and memory pressure | Predict best compaction point vs fixed thresholds | candidate novelty pending literature review |
| Incremental vs full recompute runtime switch | Dynamic analytics systems and incremental frameworks | Unified per-algorithm online work estimator with observable decision rationale | Measure prediction accuracy and total latency across update fractions | candidate novelty pending literature review |
| Affected-region execution | Incremental graph analytics | Shared runtime abstraction across several algorithms | Compare reusable runtime vs algorithm-specific implementations | known prior art / integration hypothesis |
| Adaptive intersection dispatcher | Linear, galloping, SIMD and bitmap intersections | Runtime selection using sizes, density, representation and calibrated CPU crossover | Microbenchmark across degree distributions and CPUs | integration hypothesis |
| NUMA-aware scheduling | GraphIt and CPU graph systems | Couple NUMA locality with dynamic affected regions rather than static full-graph traversal | Measure remote traffic and latency on multisocket hardware | integration hypothesis |
| Incremental triangle counting | Dynamic triangle algorithms | Reuse common-neighbor kernels and dynamic storage representation | Compare to full recount across insert/delete traces | known prior art |
| Incremental PageRank | Delta/residual PageRank work | Integrate runtime crossover and architecture-aware frontier scheduling | Compare latency, touched edges and convergence work | known prior art / integration hypothesis |
| Memory-budgeted NVMe mode | Out-of-core graph systems | Preserve versioned dynamic updates with bounded resident set | Measure latency and I/O amplification under fixed RAM | candidate novelty pending later review |

## Required record for future claims

For each proposed contribution document: idea, closest paper/system, publication date, public implementation, what it solves, limitations, VeloGraphX difference, falsifiable hypothesis, experiment, measured result, and final novelty status.

## Rule

Implementation novelty is not research novelty. A feature remains prior art unless the project demonstrates a meaningful new mechanism or empirically new systems result.