#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace velographx::kernels {
using VertexId = std::uint32_t;

enum class IntersectionKernel { scalar_merge, galloping, avx2, avx512, neon };

inline std::size_t scalar_intersection(std::span<const VertexId> a, std::span<const VertexId> b) {
  std::size_t i = 0, j = 0, count = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) { ++count; ++i; ++j; }
    else if (a[i] < b[j]) ++i;
    else ++j;
  }
  return count;
}

inline std::size_t galloping_intersection(std::span<const VertexId> small, std::span<const VertexId> large) {
  std::size_t count = 0;
  for (auto x : small) count += std::binary_search(large.begin(), large.end(), x) ? 1u : 0u;
  return count;
}

inline IntersectionKernel select_intersection(std::size_t a, std::size_t b) {
  const auto mn = std::min(a, b), mx = std::max(a, b);
  if (mn == 0) return IntersectionKernel::scalar_merge;
  if (mx > 16 * mn) return IntersectionKernel::galloping;
#if defined(__AVX512F__)
  if (mn >= 64) return IntersectionKernel::avx512;
#elif defined(__AVX2__)
  if (mn >= 32) return IntersectionKernel::avx2;
#elif defined(__ARM_NEON)
  if (mn >= 32) return IntersectionKernel::neon;
#endif
  return IntersectionKernel::scalar_merge;
}

inline std::size_t adaptive_intersection(std::span<const VertexId> a, std::span<const VertexId> b) {
  switch (select_intersection(a.size(), b.size())) {
    case IntersectionKernel::galloping:
      return a.size() <= b.size() ? galloping_intersection(a, b) : galloping_intersection(b, a);
    default:
      return scalar_intersection(a, b);
  }
}
} // namespace velographx::kernels
