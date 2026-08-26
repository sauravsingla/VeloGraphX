# Dynamic storage design

VeloGraphX uses a deliberately simple first implementation: sorted base adjacency plus per-vertex insertion and deletion deltas, with versioned batch application and threshold-driven compaction. This is a correctness-first baseline, not a novelty claim. The next experimental step is to benchmark chunked arrays, segmented adjacency, packed-memory variants, sparse/dense hybrids, and degree-adaptive layouts against this baseline using insertion/deletion throughput, neighbor scan rate, intersection throughput, memory overhead, and compaction cost.
