#include "velographx/storage/compressed_adjacency.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

int main() {
  using namespace velographx::storage;

  const std::vector<VertexId> ids{1, 2, 3, 10, 127, 128, 129, 1024, 65535, 1000000};
  assert(delta_decode(delta_encode(ids)) == ids);

  const auto varbyte = variable_byte_delta_encode(ids);
  assert(variable_byte_delta_decode(varbyte) == ids);
  assert(!varbyte.empty());
  assert(compression_ratio_bytes(ids, varbyte) > 0.0);

  const auto blocked = blocked_variable_byte_encode(ids, 3);
  assert(blocked.block_size == 3);
  assert(blocked_variable_byte_decode(blocked) == ids);
  assert(blocked.block_offsets.size() == 5);

  const auto simd = simd_friendly_delta_encode(ids, 4);
  assert(simd.block_size == 4);
  assert(simd.value_count == ids.size());
  assert(simd.blocks.size() == 3);
  assert(simd_friendly_delta_decode(simd) == ids);
  assert(compression_ratio_bytes(ids, simd) > 0.0);
  for (const auto& block : simd.blocks)
    assert(block.lane_bytes == 1 || block.lane_bytes == 2 || block.lane_bytes == 4);

  const std::vector<VertexId> dense{1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007};
  const auto dense_simd = simd_friendly_delta_encode(dense, 8);
  assert(dense_simd.blocks.size() == 1);
  assert(dense_simd.blocks[0].lane_bytes == 2); // first absolute value determines block width
  assert(simd_friendly_delta_decode(dense_simd) == dense);

  const std::vector<VertexId> tiny{1, 2, 3, 4, 5, 6, 7, 8};
  const auto tiny_simd = simd_friendly_delta_encode(tiny, 4);
  assert(tiny_simd.blocks.size() == 2);
  assert(tiny_simd.blocks[0].lane_bytes == 1);
  assert(tiny_simd.blocks[1].lane_bytes == 1);
  assert(simd_friendly_delta_decode(tiny_simd) == tiny);

  const std::vector<VertexId> empty;
  assert(variable_byte_delta_decode(variable_byte_delta_encode(empty)).empty());
  assert(blocked_variable_byte_decode(blocked_variable_byte_encode(empty, 4)).empty());
  assert(simd_friendly_delta_decode(simd_friendly_delta_encode(empty, 4)).empty());

  bool unsorted_rejected = false;
  try {
    (void)variable_byte_delta_encode(std::vector<VertexId>{5, 4});
  } catch (const std::invalid_argument&) {
    unsorted_rejected = true;
  }
  assert(unsorted_rejected);

  bool simd_unsorted_rejected = false;
  try {
    (void)simd_friendly_delta_encode(std::vector<VertexId>{5, 4}, 4);
  } catch (const std::invalid_argument&) {
    simd_unsorted_rejected = true;
  }
  assert(simd_unsorted_rejected);

  bool truncated_rejected = false;
  try {
    (void)variable_byte_delta_decode(std::vector<std::uint8_t>{0x80U});
  } catch (const std::invalid_argument&) {
    truncated_rejected = true;
  }
  assert(truncated_rejected);

  bool zero_block_rejected = false;
  try {
    (void)blocked_variable_byte_encode(ids, 0);
  } catch (const std::invalid_argument&) {
    zero_block_rejected = true;
  }
  assert(zero_block_rejected);

  bool simd_zero_block_rejected = false;
  try {
    (void)simd_friendly_delta_encode(ids, 0);
  } catch (const std::invalid_argument&) {
    simd_zero_block_rejected = true;
  }
  assert(simd_zero_block_rejected);

  auto corrupted = simd;
  corrupted.payload.pop_back();
  bool truncated_fixed_rejected = false;
  try {
    (void)simd_friendly_delta_decode(corrupted);
  } catch (const std::invalid_argument&) {
    truncated_fixed_rejected = true;
  }
  assert(truncated_fixed_rejected);

  return 0;
}
