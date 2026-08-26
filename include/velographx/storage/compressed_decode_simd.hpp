#pragma once

#include "velographx/storage/compressed_adjacency.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif
#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace velographx::storage {

namespace detail {

inline void accumulate_deltas(const std::uint32_t* deltas, std::size_t count,
                              VertexId* out) {
  VertexId value = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const auto delta = deltas[i];
    if (i == 0) {
      value = delta;
    } else {
      if (delta > std::numeric_limits<VertexId>::max() - value)
        throw std::overflow_error("vectorized fixed-width delta decode overflow");
      value += delta;
    }
    out[i] = value;
  }
}

inline std::size_t unpack_scalar(const std::uint8_t* input, std::size_t count,
                                 std::uint8_t lane_bytes, std::uint32_t* deltas) {
  for (std::size_t i = 0; i < count; ++i) {
    std::uint32_t value = 0;
    for (std::uint8_t b = 0; b < lane_bytes; ++b)
      value |= static_cast<std::uint32_t>(input[i * lane_bytes + b]) << (8U * b);
    deltas[i] = value;
  }
  return count;
}

#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
__attribute__((target("avx2")))
inline std::size_t unpack_avx2(const std::uint8_t* input, std::size_t count,
                               std::uint8_t lane_bytes, std::uint32_t* deltas) {
  std::size_t i = 0;
  if (lane_bytes == 1) {
    for (; i + 8 <= count; i += 8) {
      const __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(input + i));
      const __m256i values = _mm256_cvtepu8_epi32(bytes);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(deltas + i), values);
    }
  } else if (lane_bytes == 2) {
    for (; i + 8 <= count; i += 8) {
      const __m128i words = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i * 2));
      const __m256i values = _mm256_cvtepu16_epi32(words);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(deltas + i), values);
    }
  } else if (lane_bytes == 4) {
    for (; i + 8 <= count; i += 8) {
      const __m256i values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i * 4));
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(deltas + i), values);
    }
  }
  if (i < count) unpack_scalar(input + i * lane_bytes, count - i, lane_bytes, deltas + i);
  return count;
}

inline bool avx2_available() noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}
#else
inline bool avx2_available() noexcept { return false; }
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
inline std::size_t unpack_neon(const std::uint8_t* input, std::size_t count,
                               std::uint8_t lane_bytes, std::uint32_t* deltas) {
  std::size_t i = 0;
  if (lane_bytes == 1) {
    for (; i + 8 <= count; i += 8) {
      const uint8x8_t b = vld1_u8(input + i);
      const uint16x8_t w = vmovl_u8(b);
      vst1q_u32(deltas + i, vmovl_u16(vget_low_u16(w)));
      vst1q_u32(deltas + i + 4, vmovl_u16(vget_high_u16(w)));
    }
  } else if (lane_bytes == 2) {
    for (; i + 8 <= count; i += 8) {
      const uint16x8_t w = vld1q_u16(reinterpret_cast<const std::uint16_t*>(input + i * 2));
      vst1q_u32(deltas + i, vmovl_u16(vget_low_u16(w)));
      vst1q_u32(deltas + i + 4, vmovl_u16(vget_high_u16(w)));
    }
  } else if (lane_bytes == 4) {
    for (; i + 8 <= count; i += 8) {
      vst1q_u32(deltas + i, vld1q_u32(reinterpret_cast<const std::uint32_t*>(input + i * 4)));
      vst1q_u32(deltas + i + 4, vld1q_u32(reinterpret_cast<const std::uint32_t*>(input + (i + 4) * 4)));
    }
  }
  if (i < count) unpack_scalar(input + i * lane_bytes, count - i, lane_bytes, deltas + i);
  return count;
}
#endif

}  // namespace detail

enum class VectorDecodeBackend { scalar, avx2, neon };

inline VectorDecodeBackend vector_decode_backend() noexcept {
#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
  if (detail::avx2_available()) return VectorDecodeBackend::avx2;
#endif
#if defined(__aarch64__) || defined(__ARM_NEON)
  return VectorDecodeBackend::neon;
#else
  return VectorDecodeBackend::scalar;
#endif
}

inline std::vector<VertexId> simd_friendly_delta_decode_vectorized(
    const SimdFriendlyAdjacency& encoded) {
  std::vector<VertexId> out(encoded.value_count);
  std::size_t expected_value_offset = 0;
  std::size_t expected_payload_offset = 0;
  const auto backend = vector_decode_backend();

  for (const auto& block : encoded.blocks) {
    if (block.value_offset != expected_value_offset || block.payload_offset != expected_payload_offset)
      throw std::invalid_argument("non-contiguous fixed-width block metadata");
    if (block.value_count == 0 || block.value_offset + block.value_count > encoded.value_count)
      throw std::invalid_argument("invalid fixed-width block value bounds");
    if (block.lane_bytes != 1 && block.lane_bytes != 2 && block.lane_bytes != 4)
      throw std::invalid_argument("invalid fixed-width lane size");
    const std::size_t block_bytes = block.value_count * static_cast<std::size_t>(block.lane_bytes);
    if (block.payload_offset > encoded.payload.size() || block_bytes > encoded.payload.size() - block.payload_offset)
      throw std::invalid_argument("truncated fixed-width block");

    std::vector<std::uint32_t> deltas(block.value_count);
    const auto* input = encoded.payload.data() + block.payload_offset;
    if (backend == VectorDecodeBackend::avx2) {
#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
      detail::unpack_avx2(input, block.value_count, block.lane_bytes, deltas.data());
#else
      detail::unpack_scalar(input, block.value_count, block.lane_bytes, deltas.data());
#endif
    } else if (backend == VectorDecodeBackend::neon) {
#if defined(__aarch64__) || defined(__ARM_NEON)
      detail::unpack_neon(input, block.value_count, block.lane_bytes, deltas.data());
#else
      detail::unpack_scalar(input, block.value_count, block.lane_bytes, deltas.data());
#endif
    } else {
      detail::unpack_scalar(input, block.value_count, block.lane_bytes, deltas.data());
    }

    detail::accumulate_deltas(deltas.data(), block.value_count, out.data() + block.value_offset);
    expected_value_offset += block.value_count;
    expected_payload_offset += block_bytes;
  }

  if (expected_value_offset != encoded.value_count || expected_payload_offset != encoded.payload.size())
    throw std::invalid_argument("fixed-width adjacency metadata mismatch");
  return out;
}

}  // namespace velographx::storage
