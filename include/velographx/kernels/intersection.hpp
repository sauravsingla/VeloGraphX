#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include "velographx/kernels/cpu_features.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace velographx::kernels {

using VertexId = std::uint32_t;

enum class IntersectionKernel { scalar_merge, galloping, avx2, avx512, neon };

inline std::size_t scalar_intersection(std::span<const VertexId> a,
                                       std::span<const VertexId> b) {
  std::size_t i = 0, j = 0, count = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) {
      ++count;
      ++i;
      ++j;
    } else if (a[i] < b[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  return count;
}

inline std::size_t galloping_intersection(std::span<const VertexId> smaller,
                                          std::span<const VertexId> larger) {
  std::size_t count = 0;
  for (const auto value : smaller) {
    count += std::binary_search(larger.begin(), larger.end(), value) ? 1u : 0u;
  }
  return count;
}

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
__attribute__((target("avx2"))) inline std::size_t avx2_intersection_impl(
    std::span<const VertexId> a, std::span<const VertexId> b) {
  constexpr std::size_t lanes = 8;
  std::size_t i = 0, j = 0, count = 0;

  while (i + lanes <= a.size() && j + lanes <= b.size()) {
    const auto vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b.data() + j));
    std::uint32_t matched_mask = 0;
    for (std::size_t lane = 0; lane < lanes; ++lane) {
      const auto va = _mm256_set1_epi32(static_cast<int>(a[i + lane]));
      const auto eq = _mm256_cmpeq_epi32(va, vb);
      const auto mask = static_cast<unsigned>(_mm256_movemask_ps(_mm256_castsi256_ps(eq)));
      if (mask != 0) {
        matched_mask |= static_cast<std::uint32_t>(1u << lane);
      }
    }
    count += std::popcount(matched_mask);

    const auto a_last = a[i + lanes - 1];
    const auto b_last = b[j + lanes - 1];
    if (a_last <= b_last) i += lanes;
    if (b_last <= a_last) j += lanes;
  }

  count += scalar_intersection(a.subspan(i), b.subspan(j));
  return count;
}

__attribute__((target("avx512f"))) inline std::size_t avx512_intersection_impl(
    std::span<const VertexId> a, std::span<const VertexId> b) {
  constexpr std::size_t lanes = 16;
  std::size_t i = 0, j = 0, count = 0;

  while (i + lanes <= a.size() && j + lanes <= b.size()) {
    const auto vb = _mm512_loadu_si512(reinterpret_cast<const void*>(b.data() + j));
    std::uint32_t matched_mask = 0;
    for (std::size_t lane = 0; lane < lanes; ++lane) {
      const auto va = _mm512_set1_epi32(static_cast<int>(a[i + lane]));
      if (_mm512_cmpeq_epu32_mask(va, vb) != 0) {
        matched_mask |= static_cast<std::uint32_t>(1u << lane);
      }
    }
    count += std::popcount(matched_mask);

    const auto a_last = a[i + lanes - 1];
    const auto b_last = b[j + lanes - 1];
    if (a_last <= b_last) i += lanes;
    if (b_last <= a_last) j += lanes;
  }

  count += scalar_intersection(a.subspan(i), b.subspan(j));
  return count;
}
#endif

inline std::size_t avx2_intersection(std::span<const VertexId> a,
                                     std::span<const VertexId> b) {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
  if (detect_cpu_features().avx2) return avx2_intersection_impl(a, b);
#endif
  return scalar_intersection(a, b);
}

inline std::size_t avx512_intersection(std::span<const VertexId> a,
                                       std::span<const VertexId> b) {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
  if (detect_cpu_features().avx512f) return avx512_intersection_impl(a, b);
#endif
  return scalar_intersection(a, b);
}

inline std::size_t neon_intersection(std::span<const VertexId> a,
                                     std::span<const VertexId> b) {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  constexpr std::size_t lanes = 4;
  std::size_t i = 0, j = 0, count = 0;
  while (i + lanes <= a.size() && j + lanes <= b.size()) {
    const auto vb = vld1q_u32(b.data() + j);
    for (std::size_t lane = 0; lane < lanes; ++lane) {
      const auto va = vdupq_n_u32(a[i + lane]);
      const auto eq = vceqq_u32(va, vb);
      const auto pair = vpaddq_u32(eq, eq);
      const auto reduced = vpaddq_u32(pair, pair);
      if (vgetq_lane_u32(reduced, 0) != 0) ++count;
    }
    const auto a_last = a[i + lanes - 1];
    const auto b_last = b[j + lanes - 1];
    if (a_last <= b_last) i += lanes;
    if (b_last <= a_last) j += lanes;
  }
  count += scalar_intersection(a.subspan(i), b.subspan(j));
  return count;
#else
  return scalar_intersection(a, b);
#endif
}

inline IntersectionKernel select_intersection(std::size_t a_size, std::size_t b_size) {
  const auto min_size = std::min(a_size, b_size);
  const auto max_size = std::max(a_size, b_size);
  if (min_size == 0) return IntersectionKernel::scalar_merge;
  if (max_size > 16 * min_size) return IntersectionKernel::galloping;

  const auto features = detect_cpu_features();
  if (features.avx512f && min_size >= 64) return IntersectionKernel::avx512;
  if (features.avx2 && min_size >= 32) return IntersectionKernel::avx2;
  if (features.neon && min_size >= 32) return IntersectionKernel::neon;
  return IntersectionKernel::scalar_merge;
}

inline std::size_t adaptive_intersection(std::span<const VertexId> a,
                                         std::span<const VertexId> b) {
  switch (select_intersection(a.size(), b.size())) {
    case IntersectionKernel::galloping:
      return a.size() <= b.size() ? galloping_intersection(a, b)
                                  : galloping_intersection(b, a);
    case IntersectionKernel::avx2:
      return avx2_intersection(a, b);
    case IntersectionKernel::avx512:
      return avx512_intersection(a, b);
    case IntersectionKernel::neon:
      return neon_intersection(a, b);
    default:
      return scalar_intersection(a, b);
  }
}

}  // namespace velographx::kernels
