#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace velographx::storage {

using VertexId = std::uint32_t;

inline std::vector<std::uint32_t> delta_encode(const std::vector<VertexId>& ids) {
  std::vector<std::uint32_t> out;
  out.reserve(ids.size());
  VertexId prev = 0;
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i && ids[i] < ids[i - 1]) throw std::invalid_argument("adjacency must be sorted");
    out.push_back(i ? ids[i] - prev : ids[i]);
    prev = ids[i];
  }
  return out;
}

inline std::vector<VertexId> delta_decode(const std::vector<std::uint32_t>& deltas) {
  std::vector<VertexId> out;
  out.reserve(deltas.size());
  VertexId value = 0;
  for (std::size_t i = 0; i < deltas.size(); ++i) {
    if (i == 0) {
      value = deltas[i];
    } else {
      if (deltas[i] > std::numeric_limits<VertexId>::max() - value)
        throw std::overflow_error("delta decode overflow");
      value += deltas[i];
    }
    out.push_back(value);
  }
  return out;
}

inline void variable_byte_encode_uint32(std::uint32_t value, std::vector<std::uint8_t>& out) {
  while (value >= 0x80U) {
    out.push_back(static_cast<std::uint8_t>((value & 0x7FU) | 0x80U));
    value >>= 7U;
  }
  out.push_back(static_cast<std::uint8_t>(value));
}

inline std::uint32_t variable_byte_decode_uint32(const std::vector<std::uint8_t>& data,
                                                 std::size_t& offset) {
  std::uint32_t value = 0;
  unsigned shift = 0;
  while (offset < data.size()) {
    const auto byte = data[offset++];
    if (shift >= 32U && (byte & 0x7FU) != 0U) throw std::overflow_error("varbyte decode overflow");
    value |= static_cast<std::uint32_t>(byte & 0x7FU) << shift;
    if ((byte & 0x80U) == 0U) return value;
    shift += 7U;
    if (shift > 28U) throw std::overflow_error("invalid varbyte uint32");
  }
  throw std::invalid_argument("truncated varbyte stream");
}

inline std::vector<std::uint8_t> variable_byte_delta_encode(const std::vector<VertexId>& ids) {
  const auto deltas = delta_encode(ids);
  std::vector<std::uint8_t> out;
  out.reserve(deltas.size() * 2U);
  for (auto delta : deltas) variable_byte_encode_uint32(delta, out);
  return out;
}

inline std::vector<VertexId> variable_byte_delta_decode(const std::vector<std::uint8_t>& data) {
  std::vector<std::uint32_t> deltas;
  std::size_t offset = 0;
  while (offset < data.size()) deltas.push_back(variable_byte_decode_uint32(data, offset));
  return delta_decode(deltas);
}

struct BlockedAdjacency {
  std::size_t block_size{128};
  std::vector<std::size_t> block_offsets;
  std::vector<std::uint8_t> payload;
  std::size_t value_count{0};
};

inline BlockedAdjacency blocked_variable_byte_encode(const std::vector<VertexId>& ids,
                                                     std::size_t block_size = 128) {
  if (block_size == 0) throw std::invalid_argument("block size must be positive");
  BlockedAdjacency result;
  result.block_size = block_size;
  result.value_count = ids.size();
  result.block_offsets.push_back(0);

  for (std::size_t first = 0; first < ids.size(); first += block_size) {
    const std::size_t last = (first + block_size < ids.size()) ? first + block_size : ids.size();
    std::vector<VertexId> block(ids.begin() + static_cast<std::ptrdiff_t>(first),
                                ids.begin() + static_cast<std::ptrdiff_t>(last));
    const auto encoded = variable_byte_delta_encode(block);
    result.payload.insert(result.payload.end(), encoded.begin(), encoded.end());
    result.block_offsets.push_back(result.payload.size());
  }
  return result;
}

inline std::vector<VertexId> blocked_variable_byte_decode(const BlockedAdjacency& encoded) {
  if (encoded.block_offsets.empty() || encoded.block_offsets.front() != 0)
    throw std::invalid_argument("invalid blocked adjacency offsets");
  std::vector<VertexId> out;
  out.reserve(encoded.value_count);
  for (std::size_t block = 0; block + 1 < encoded.block_offsets.size(); ++block) {
    const auto begin = encoded.block_offsets[block];
    const auto end = encoded.block_offsets[block + 1];
    if (begin > end || end > encoded.payload.size()) throw std::invalid_argument("invalid block bounds");
    std::vector<std::uint8_t> bytes(encoded.payload.begin() + static_cast<std::ptrdiff_t>(begin),
                                    encoded.payload.begin() + static_cast<std::ptrdiff_t>(end));
    const auto decoded = variable_byte_delta_decode(bytes);
    out.insert(out.end(), decoded.begin(), decoded.end());
  }
  if (out.size() != encoded.value_count) throw std::invalid_argument("blocked adjacency count mismatch");
  return out;
}

inline double compression_ratio_bytes(const std::vector<VertexId>& ids,
                                      const std::vector<std::uint8_t>& encoded) noexcept {
  if (ids.empty()) return encoded.empty() ? 1.0 : 0.0;
  const auto raw_bytes = static_cast<double>(ids.size() * sizeof(VertexId));
  return raw_bytes / static_cast<double>(encoded.empty() ? 1 : encoded.size());
}

}  // namespace velographx::storage
