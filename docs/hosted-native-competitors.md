# Hosted native competitor evidence

This workflow implements the native BFS/SSSP methodology recommended by Prof. Timothy A. Davis for evaluating VeloGraphX against both LAGraph/SuiteSparse:GraphBLAS and the GAP Benchmark Suite.

## Pinned external systems

- SuiteSparse:GraphBLAS `v10.5.0`, the current stable release at the time this contract was implemented
- LAGraph development branch `v1.3.x`, pinned to commit `d01064de77b606473744b99f63b1487963556194`
- GAP Benchmark Suite `v1.5`

Exact resolved commits are retained with every artifact. Hosted evidence remains `research_claim: false` and `publication_grade: false`.

Accepted hosted evidence: GitHub Actions run `33418520303`, artifact `9768499895`. The retained campaign passed the cross-engine correctness gates for BFS, SSSP and dynamic snapshots and is summarized in the README. It is the current hosted engineering result, not a dedicated-hardware publication result.

## Davis methodology applied

The campaign follows these rules:

- benchmark both LAGraph/GraphBLAS **and** GAP for BFS and SSSP
- use `OMP_PLACES=cores` and `OMP_PROC_BIND=spread`
- keep the same graph, source vertex, thread count, runner and repetition policy for comparable measurements
- exclude input-file loading and one-time representation construction from the primary static kernel timing
- retain process-wall timing separately so loading/conversion costs are visible rather than silently mixed with kernel timing
- enable LAGraph's independent benchmark self-checks
- use GAP's built-in serial verification for every measured run
- independently verify VeloGraphX weighted SSSP against a serial Dijkstra oracle
- retain raw per-repetition timings before computing medians
- report results where competitors win as well as results where VeloGraphX wins

The LAGraph source-node MatrixMarket file contains the same explicit source used by VeloGraphX and GAP. This removes the earlier limitation where LAGraph's benchmark demo selected its own source set.

## Static campaign

The hosted engineering campaign executes BFS and weighted SSSP at 1, 2 and 4 threads. Each engine is measured five times per configuration.

For BFS, VeloGraphX and GAP consume the same generated undirected edge set, while LAGraph consumes the equivalent symmetric MatrixMarket representation. For SSSP, all three engines consume the same deterministic positive integer edge weights; GAP and LAGraph read the weighted MatrixMarket representation and VeloGraphX reads the equivalent weighted edge list.

Primary timing is kernel-only. Input loading, GraphBLAS matrix construction, LAGraph cached properties/transposes and GAP graph construction are outside the static kernel timer. Their process-wall times are retained separately.

## Dynamic crossover campaign

Hosted CI also evaluates deterministic addition batches at 0.1%, 1% and 5% of the base edge count, at one thread.

For each update fraction:

1. VeloGraphX loads the base graph, then times update application plus exact incremental repair.
2. VeloGraphX independently times a fresh full recomputation on the post-update in-memory graph.
3. The exact same post-update graph is materialized as an immutable snapshot for GAP and LAGraph.
4. GAP and LAGraph execute optimized full recomputation on that snapshot with their own correctness checks.
5. Both kernel-only recomputation ratios and separately labelled process-wall/end-to-end ratios are retained.

The process-wall external ratio is deliberately labelled separately because it includes fresh input loading and representation construction. It must not be mixed with the primary kernel-only comparison.

## Evidence schema

The retained artifact records:

- VeloGraphX commit via GitHub workflow provenance
- GraphBLAS tag and resolved commit
- LAGraph branch and immutable commit
- GAP tag and resolved commit
- compiler/runtime and CPU topology
- OpenMP placement/binding settings
- dataset and update-snapshot SHA-256 checksums
- source vertex
- thread counts
- five raw repetitions
- kernel and process-wall timing scopes
- exactness/self-check status for every engine
- dynamic update fractions and counts
- VeloGraphX-vs-full-recompute crossover ratios
- VeloGraphX-vs-GAP and VeloGraphX-vs-LAGraph ratios

## Claim policy

GitHub-hosted timing is reproducible engineering evidence, not publication-grade scalability evidence. Dedicated controlled hardware is still required for 8/16/32+ core scaling, genuine NUMA evaluation, stable hardware counters and publication-grade performance claims.

README performance claims should be added only after the corresponding retained artifact has completed successfully and the result has been reviewed for runner variance and workload scope.
