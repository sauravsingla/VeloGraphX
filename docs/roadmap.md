# Roadmap

VeloGraphX now implements the broad architectural surfaces from the original prompt: weighted mutable graphs, localized exact repair with safe recomputation fallback, runtime-dispatched SIMD paths, NUMA-aware placement and scheduling, NumPy/SciPy/Arrow interoperability, timestamped history, compression codecs, and partitioned mmap/async/`io_uring` storage. Teseo, Boost.Graph and Sortledton exercise the storage-independent graph contract; pinned NetworKit, RisGraph, GAP and LAGraph workflows provide correctness-gated hosted engineering evidence.

The remaining roadmap is therefore validation-led rather than feature-checklist-led:

1. Run the unified canonical publication campaign on dedicated hardware across checksum-pinned web, social/community and road graphs, the full Kronecker/R-MAT series, and the largest clean in-memory boundary.
2. Establish stable 1/2/4/8/16/32+ thread scaling, genuine multi-socket NUMA behavior and hardware-counter evidence under controlled affinity and frequency settings.
3. Execute same-hardware competitor campaigns, including RisGraph/NetworKit dynamic BFS and GAP/LAGraph static kernels, without combining incompatible timing contracts.
4. Complete research-scale update-fraction crossover, selector ablation, compression calibration and repeated 100M+-edge campaigns on public datasets.
5. Measure the partitioned backend on dedicated NVMe hardware, including bounded-cache behavior and `io_uring` overlap.
6. Broaden weighted-dynamic evaluation, stabilize the long-term Python API, obtain independent reproduction and turn the retained evidence into a focused systems paper.

Hosted-CI results remain engineering evidence. Publication claims stay gated until the dedicated campaign records immutable datasets, machine/toolchain metadata, raw repetitions, correctness signatures and retained artifacts.
