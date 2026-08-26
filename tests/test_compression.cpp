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

  const std::vector<VertexId> empty;
  assert(variable_byte_delta_decode(variable_byte_delta_encode(empty)).empty());
  assert(blocked_variable_byte_decode(blocked_variable_byte_encode(empty, 4)).empty());

  bool unsorted_rejected = false;
  try {
    (void)variable_byte_delta_encode(std::vector<VertexId>{5, 4});
  } catch (const std::invalid_argument&) {
    unsorted_rejected = true;
  }
  assert(unsorted_rejected);

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

  return 0;
}
