# Same-run published exact triangle reference comparison

This document records a hosted-CI comparison between VeloGraphX and an **unmodified exact reference counter distributed with published SIGMOD 2021 source code**. It is engineering evidence only: `research_claim=false` and `publication_grade=false`.

## Published reference

The comparison uses the public repository [`StreamingTriangleCounting/TriangleCounting`](https://github.com/StreamingTriangleCounting/TriangleCounting), source code accompanying *Sliding Window-based Approximate Triangle Counting over Streaming Graphs with Duplicate Edges* (SIGMOD 2021), pinned at commit:

`1085ba049bb94451661d119284d7cd9b68687a81`

The exact undirected reference component is:

`TKDE-triangle-code/Golden-triangle/undirected/GoldenCounter.h`

with `Graph.h` from the same pinned revision. CI clones that immutable revision at run time and verifies the exact checkout. VeloGraphX does not vendor or modify the published source.

An important semantic distinction is preserved. `GoldenCounter::insert_edge(...)` dynamically updates its graph representation, while `GoldenCounter::triangle_count()` computes the exact global triangle count by scanning that graph. Therefore the fair **exact-answer-ready** baseline latency is:

`GoldenCounter insertion latency + GoldenCounter exact triangle_count() query latency`

The workflow also records GoldenCounter update-only latency separately, but it is **not** treated as equivalent to VeloGraphX's update latency because the exact triangle answer is not yet available after GoldenCounter insertion alone.

## Normalized comparison contract

Workflow: `.github/workflows/same-run-published-baseline.yml`

Run: GitHub Actions `33248107299`

VeloGraphX revision measured: `fb90cacb05d31bd9e62397f04cacf0d14662ac31`

Both implementations are exercised inside the same comparison process on the same GitHub-hosted Ubuntu runner:

- Linux x86_64, 4 logical CPUs;
- GCC 13.3.0;
- `-O3 -DNDEBUG -std=c++20` for the same-run comparator;
- identical normalized `facebook-combined` graph;
- 4,039 vertices / 88,234 undirected edges;
- normalized graph SHA-256 `475b986afae8f4d4dcf96537768fbb159db68f1ac23be5b715a8ed90c8f59641`;
- identical deterministic missing-edge insertion batches;
- identical requested batch sizes at 1%, 5%, and 10% of base edges;
- five independent repetitions per fraction;
- exact VeloGraphX incremental count, published GoldenCounter count, and VeloGraphX full recomputation must all agree for every sample.

All 15 comparisons passed the exact-count gate.

## Results

Values below are medians over five repetitions. `GoldenCounter exact-answer-ready` includes both its insertion time and exact `triangle_count()` query time.

| Update batch | VeloGraphX exact-answer-ready | VeloGraphX throughput | Published GoldenCounter exact-answer-ready | GoldenCounter throughput | GoldenCounter / VeloGraphX answer-ready latency | VeloGraphX full recompute | VeloGraphX incremental / full |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1% / 883 edges | **1.066 ms** | **0.828 M updates/s** | 43.657 ms | 0.0202 M updates/s | **40.95x** | 87.900 ms | **82.45x** |
| 5% / 4,412 edges | **6.782 ms** | **0.651 M updates/s** | 47.095 ms | 0.0937 M updates/s | **6.94x** | 122.428 ms | **18.05x** |
| 10% / 8,824 edges | **15.495 ms** | **0.569 M updates/s** | 53.931 ms | 0.164 M updates/s | **3.48x** | 142.236 ms | **9.18x** |

For additional transparency, the published reference's median **update-only** times were 0.602 ms, 2.503 ms, and 5.184 ms at 1%, 5%, and 10%, respectively. Those values are intentionally not used as the headline comparison because the exact triangle count still requires its subsequent query (43.055 ms, 44.661 ms, and 48.747 ms median, respectively).

## Interpretation boundary

This is a stronger comparison than quoting timing numbers from a paper because the two implementations execute on the same runner, compiler, graph and update sequence. It is still **not** a claim that VeloGraphX is faster than the SIGMOD 2021 SWTC algorithm itself: SWTC is an approximate sliding-window algorithm with different semantics, while the comparator here is the paper repository's exact `GoldenCounter` reference implementation.

The defensible statement is narrower:

> On the exercised GitHub-hosted `facebook-combined` insertion workloads, VeloGraphX produced the same exact post-update triangle count as the pinned published GoldenCounter reference while reaching that exact answer with 40.95x, 6.94x, and 3.48x lower median answer-ready latency at 1%, 5%, and 10% update batches, respectively.

The result remains hosted-CI engineering evidence. Publication-grade claims still require controlled dedicated hardware, broader/larger datasets, repeated uncertainty analysis, and comparable optimized systems under a pre-registered execution contract.
