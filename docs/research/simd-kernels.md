# SIMD and adaptive graph kernels

The kernel API separates policy from implementation. The current dispatcher distinguishes scalar merge and galloping intersection and records compile-time availability for AVX2, AVX-512, and NEON paths. Architecture-specific vector kernels remain guarded behind the same API so correctness can be differential-tested against scalar code. Crossover thresholds must be calibrated by microbenchmarks rather than presented as universal constants.
