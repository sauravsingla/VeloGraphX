#pragma once

#include "velographx/storage/compressed_adjacency.hpp"
#include "velographx/storage/compressed_decode_simd.hpp"

#include <cstddef>
#include <vector>

namespace velographx::storage {

enum class CompressionCodec {
  variable_byte,
  simd_friendly_fixed_width,
};

struct CompressionRecommendation {
  CompressionCodec codec{CompressionCodec::variable_byte};
  std::size_t variable_byte_bytes{0};
  std::size_t fixed_width_bytes{0};
  VectorDecodeBackend vector_backend{VectorDecodeBackend::scalar};
  double fixed_width_overhead_ratio{1.0};
};

inline CompressionRecommendation recommend_compression_codec(
    const std::vector<VertexId>& ids,
    std::size_t block_size = 128,
    double max_fixed_width_overhead = 1.10) {
  if (max_fixed_width_overhead < 1.0)
    throw std::invalid_argument("max fixed-width overhead must be at least 1.0");

  const auto varbyte = variable_byte_delta_encode(ids);
  const auto fixed = simd_friendly_delta_encode(ids, block_size);
  const auto backend = vector_decode_backend();

  const auto varbyte_bytes = varbyte.size();
  const auto fixed_bytes = fixed.payload.size();
  const double overhead = varbyte_bytes == 0
      ? 1.0
      : static_cast<double>(fixed_bytes) / static_cast<double>(varbyte_bytes);

  CompressionCodec selected = CompressionCodec::variable_byte;
  if (fixed_bytes < varbyte_bytes) {
    selected = CompressionCodec::simd_friendly_fixed_width;
  } else if (backend != VectorDecodeBackend::scalar &&
             !ids.empty() &&
             overhead <= max_fixed_width_overhead) {
    selected = CompressionCodec::simd_friendly_fixed_width;
  }

  return {selected, varbyte_bytes, fixed_bytes, backend, overhead};
}

}  // namespace velographx::storage
