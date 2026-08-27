# Hosted CI evidence campaign

This note records the successful GitHub-hosted **CI-scale Evidence Campaign #4** for VeloGraphX. It is engineering evidence only, not publication-grade performance evidence.

## Provenance

- VeloGraphX commit: `651312c37a7b982001ef1aace17b6f28547cf19c`
- Runner: GitHub-hosted Ubuntu 24.04, x86_64
- Logical CPUs reported: 4
- Python: 3.12.14
- GCC: 13.3.0
- Clang: 18.1.3
- Python competitors: NetworkX 3.6.1, igraph 1.0.0, NetworKit 11.2.1, rustworkx 0.18.1
- LAGraph/GraphBLAS and GAP native binaries: not installed in this run
- Dataset fixture SHA-256: `b2d516ac2fbfdf330a3807ea2448b0f4c0e9f82abd7a9eefdb1522aa364ec48e`
- Research claim: `false`

The result bundle passed the benchmark preflight and integrity validator before artifact upload.

## What the campaign established

### Correctness and cross-framework agreement

The builtin BFS reference, NetworkX, igraph, NetworKit and rustworkx produced the same normalized BFS result digest on the identical fixture. This validates the current adapter normalization path for this CI fixture; it is not a performance ranking.

### Incremental triangle-update behavior

The update-fraction campaign ran five repetitions at update fractions `1e-6`, `1e-5`, `1e-4`, `1e-3`, `1e-2`, `0.05` and `0.10` on a 20,000-vertex / 60,000-base-edge synthetic graph.

For fractions where the incremental timer resolved above zero, the median reported full-recompute/incremental speedup was approximately:

| Update fraction | Median reported speedup |
| ---: | ---: |
| 0.0001 | 3838x |
| 0.001 | 318.6x |
| 0.01 | 30.4x |
| 0.05 | 6.79x |
| 0.10 | 3.96x |

The two smallest fractions frequently measured `0 us` for the incremental path, so their speedup field is not numerically meaningful and is intentionally not summarized as a result.

These values are useful for smoke-level crossover behavior on this runner only. They should not be cited as general VeloGraphX speedups until repeated on pinned public datasets and dedicated hardware.

### Intersection-kernel behavior

The runner reported AVX2 as the best available ISA. On the included synthetic intersection cases, the adaptive kernel was faster than scalar in five of six tested size pairs, with scalar/adaptive ratios ranging from roughly 1.47x to 5.40x in those cases. For the smallest 8x8 case, scalar was faster than adaptive.

This is evidence that the adaptive dispatch is selecting useful optimized paths for several representative size regimes, while retaining an important small-input case where dispatch overhead can dominate.

### Compression behavior

For the fixed-width codec, the vectorized decoder was faster than the scalar decoder on all three generated families in the standalone campaign:

| Family | Scalar/vectorized decode ratio |
| --- | ---: |
| dense | ~2.69x |
| medium | ~3.66x |
| sparse | ~2.93x |

The same data also shows the expected compression trade-off: variable-byte coding produced substantially smaller encoded output than fixed-width coding on these fixtures, while fixed-width vectorization improved decode speed. These are engineering measurements, not a universal codec recommendation.

### Thread-scaling smoke check

The campaign exercised 1-thread and 2-thread affinity-constrained runs successfully on a 4-logical-CPU hosted runner. The static benchmark timings did not show a stable 2-thread advantage in this tiny workload, so no scaling claim is made from this CI experiment.

## What this does not establish

This campaign does **not** establish publication-grade superiority, large-graph scalability, multi-socket NUMA behavior, 8/16/32+ thread scaling, native LAGraph/GAP comparisons, 100M+ edge performance, hardware-counter conclusions, NVMe/`io_uring` throughput, or public-dataset results.

Those remain dedicated-hardware / research-campaign tasks and should continue to use the repository's dataset, version-pin, environment, preflight and result-artifact contracts.

## Reproduction

Run the GitHub Actions workflow `.github/workflows/ci-scale-evidence.yml` or dispatch **CI-scale Evidence Campaign** manually. The workflow builds the benchmarks, runs correctness tests, captures environment and dataset provenance, executes the hosted-CI campaign, validates the result bundle, and uploads `velographx-ci-scale-evidence`.
