#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "velographx/storage/partition_cache.hpp"
#include "velographx/storage/partition_file.hpp"

namespace velographx {

class AsyncPartitionLoader {
 public:
  explicit AsyncPartitionLoader(std::size_t resident_bytes)
      : cache_(resident_bytes) {}

  std::future<std::vector<std::uint8_t>> prefetch(
      std::uint64_t partition_id,
      std::filesystem::path path) {
    return std::async(std::launch::async,
                      [this, partition_id, path = std::move(path)]() mutable {
      if (auto cached = cache_.get(partition_id)) return *cached;
      auto payload = PartitionFile::read_mmap_or_fallback(path, partition_id);
      cache_.put(partition_id, payload);
      return payload;
    });
  }

  std::vector<std::uint8_t> load(std::uint64_t partition_id,
                                 const std::filesystem::path& path) {
    if (auto cached = cache_.get(partition_id)) return *cached;
    auto payload = PartitionFile::read_mmap_or_fallback(path, partition_id);
    cache_.put(partition_id, payload);
    return payload;
  }

  [[nodiscard]] const PartitionCacheStats& stats() const noexcept {
    return cache_.stats();
  }

  [[nodiscard]] std::size_t resident_bytes() const noexcept {
    return cache_.resident_bytes();
  }

 private:
  PartitionCache cache_;
};

}  // namespace velographx
