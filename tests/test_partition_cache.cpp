#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "velographx/storage/partition_cache.hpp"

int main() {
  using velographx::PartitionCache;

  PartitionCache<> cache(100);
  cache.put(1, std::vector<std::uint8_t>(40, 1), 40);
  cache.put(2, std::vector<std::uint8_t>(40, 2), 40);
  assert(cache.size() == 2);
  assert(cache.resident_bytes() == 80);

  assert(cache.get(1) != nullptr);  // make partition 1 most recently used
  cache.put(3, std::vector<std::uint8_t>(40, 3), 40);
  assert(cache.contains(1));
  assert(!cache.contains(2));
  assert(cache.contains(3));
  assert(cache.stats().evictions == 1);
  assert(cache.stats().hits == 1);

  assert(cache.get(99) == nullptr);
  assert(cache.stats().misses == 1);

  bool threw = false;
  try {
    cache.put(4, std::vector<std::uint8_t>(101), 101);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);

  cache.erase(1);
  assert(!cache.contains(1));
  cache.clear();
  assert(cache.size() == 0);
  assert(cache.resident_bytes() == 0);
  return 0;
}
