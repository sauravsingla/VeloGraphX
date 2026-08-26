# Ablation plan

The evaluation matrix must compare scalar vs architecture-specific kernels, fixed vs adaptive intersection, CSR-only vs CSR+delta, incremental vs recompute, single-thread vs multicore scheduling, NUMA-aware vs unaware allocation, sparse vs dense frontiers, push vs pull, compression vs uncompressed storage, and prefetch on/off. Negative results are retained. Update fractions are 0.0001%, 0.001%, 0.01%, 0.1%, 1%, 5% and 10%, with explicit crossover reporting.
