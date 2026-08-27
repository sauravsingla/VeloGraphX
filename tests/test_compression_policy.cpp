#include "velographx/storage/compression_policy.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

int main() {
  using namespace velographx::storage;

  const std::vector<VertexId> dense{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  const auto dense_choice = recommend_compression_codec(dense, 16, 1.25);
  assert(dense_choice.variable_byte_bytes > 0);
  assert(dense_choice.fixed_width_bytes > 0);
  assert(dense_choice.fixed_width_overhead_ratio > 0.0);

  const std::vector<VertexId> sparse{1, 100000, 200000, 300000, 400000, 500000};
  const auto sparse_choice = recommend_compression_codec(sparse, 6, 1.0);
  if (sparse_choice.fixed_width_bytes < sparse_choice.variable_byte_bytes)
    assert(sparse_choice.codec == CompressionCodec::simd_friendly_fixed_width);
  else
    assert(sparse_choice.codec == CompressionCodec::variable_byte);

  const std::vector<VertexId> empty;
  const auto empty_choice = recommend_compression_codec(empty);
  assert(empty_choice.codec == CompressionCodec::variable_byte);
  assert(empty_choice.variable_byte_bytes == 0);
  assert(empty_choice.fixed_width_bytes == 0);

  bool rejected = false;
  try { (void)recommend_compression_codec(dense, 16, 0.99); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);

  bool zero_block_rejected = false;
  try { (void)recommend_compression_codec(dense, 0); }
  catch (const std::invalid_argument&) { zero_block_rejected = true; }
  assert(zero_block_rejected);

  return 0;
}
