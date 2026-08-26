# Compression baseline

VeloGraphX now includes reversible sorted-adjacency delta encoding as a correctness baseline. It is not enabled by default and makes no compression-performance claim. Future experiments should compare uncompressed storage against delta+variable-byte and SIMD-friendly blocked codecs using bytes/edge, decode throughput, traversal throughput and update complexity.
