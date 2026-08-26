#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <utility>
#include <vector>

#include "velographx/storage/file_prefetch.hpp"
#include "velographx/storage/io_uring_prefetch.hpp"
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
      if (auto* cached = cache_.get(static_cast<PartitionId>(partition_id))) return *cached;
      advise_prefetch(path);
      auto payload = PartitionFile::read_mmap_or_fallback(path, partition_id);
      const auto bytes = payload.size();
      cache_.put(static_cast<PartitionId>(partition_id), payload, bytes);
      return payload;
    });
  }

  std::vector<std::uint8_t> load(std::uint64_t partition_id,
                                 const std::filesystem::path& path) {
    if (auto* cached = cache_.get(static_cast<PartitionId>(partition_id))) return *cached;
    advise_prefetch(path);
    auto payload = PartitionFile::read_mmap_or_fallback(path, partition_id);
    const auto bytes = payload.size();
    cache_.put(static_cast<PartitionId>(partition_id), payload, bytes);
    return payload;
  }

  [[nodiscard]] const PartitionCacheStats& stats() const noexcept { return cache_.stats(); }
  [[nodiscard]] std::size_t resident_bytes() const noexcept { return cache_.resident_bytes(); }
  [[nodiscard]] FilePrefetchResult last_prefetch_result() const noexcept { return last_prefetch_; }
  [[nodiscard]] IoUringPrefetchResult last_io_uring_result() const noexcept { return last_io_uring_; }

  [[nodiscard]] static constexpr bool native_prefetch_supported() noexcept {
    return FilePrefetchAdvisor::supported();
  }
  [[nodiscard]] static constexpr bool io_uring_prefetch_compiled() noexcept {
    return IoUringPrefetchAdvisor::compiled();
  }

 private:
  void advise_prefetch(const std::filesystem::path& path) {
    last_io_uring_ = IoUringPrefetchAdvisor::prefetch(path);
    if (!last_io_uring_.succeeded) last_prefetch_ = FilePrefetchAdvisor::advise_will_need(path);
    else last_prefetch_ = {};
  }

  PartitionCache<> cache_;
  FilePrefetchResult last_prefetch_{};
  IoUringPrefetchResult last_io_uring_{};
};

}  // namespace velographx
