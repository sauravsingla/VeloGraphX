# Memory-budget and out-of-core design

The initial repository exposes an in-memory engine. The out-of-core contract is defined around explicit partition residency rather than rescanning source files. A future `MemoryBudget` controller will account for base adjacency, deltas, algorithm state and cache, and will choose resident/cold partitions. Linux io_uring and mmap are optional implementation mechanisms, not API requirements. This document intentionally distinguishes implemented in-memory functionality from the future NVMe execution milestone.
