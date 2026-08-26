#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "velographx/storage/memory_budget.hpp"

namespace velographx {

using PartitionId = std::uint32_t;

struct PartitionCacheStats {
  std::size_t resident_bytes{0};
  std::size_t hits{0};
  std::size_t misses{0};
  std::size_t evictions{0};
};

template <class Payload = std::vector<std::uint8_t>>
class PartitionCache {
 public:
  explicit PartitionCache(MemoryBudget budget, std::size_t algorithm_state_bytes = 0)
      : capacity_(budget.resident_limit(budget.bytes(), algorithm_state_bytes)) {}

  explicit PartitionCache(std::size_t capacity_bytes) : capacity_(capacity_bytes) {}

  [[nodiscard]] std::size_t capacity_bytes() const noexcept { return capacity_; }
  [[nodiscard]] std::size_t resident_bytes() const noexcept { return stats_.resident_bytes; }
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] const PartitionCacheStats& stats() const noexcept { return stats_; }

  void clear() {
    entries_.clear();
    lru_.clear();
    stats_.resident_bytes = 0;
  }

  [[nodiscard]] bool contains(PartitionId id) const noexcept {
    return entries_.find(id) != entries_.end();
  }

  const Payload* get(PartitionId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
      ++stats_.misses;
      return nullptr;
    }
    ++stats_.hits;
    touch(it);
    return &it->second.payload;
  }

  void put(PartitionId id, Payload payload, std::size_t bytes) {
    if (bytes > capacity_) {
      throw std::length_error("partition exceeds cache capacity");
    }

    auto existing = entries_.find(id);
    if (existing != entries_.end()) {
      stats_.resident_bytes -= existing->second.bytes;
      lru_.erase(existing->second.lru_it);
      entries_.erase(existing);
    }

    while (stats_.resident_bytes + bytes > capacity_ && !lru_.empty()) {
      evict_one();
    }

    lru_.push_front(id);
    entries_.emplace(id, Entry{std::move(payload), bytes, lru_.begin()});
    stats_.resident_bytes += bytes;
  }

  bool erase(PartitionId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return false;
    stats_.resident_bytes -= it->second.bytes;
    lru_.erase(it->second.lru_it);
    entries_.erase(it);
    return true;
  }

 private:
  struct Entry {
    Payload payload;
    std::size_t bytes{0};
    typename std::list<PartitionId>::iterator lru_it;
  };

  using Map = std::unordered_map<PartitionId, Entry>;

  void touch(typename Map::iterator it) {
    lru_.erase(it->second.lru_it);
    lru_.push_front(it->first);
    it->second.lru_it = lru_.begin();
  }

  void evict_one() {
    const auto victim = lru_.back();
    lru_.pop_back();
    auto it = entries_.find(victim);
    if (it != entries_.end()) {
      stats_.resident_bytes -= it->second.bytes;
      entries_.erase(it);
      ++stats_.evictions;
    }
  }

  std::size_t capacity_{0};
  std::list<PartitionId> lru_;
  Map entries_;
  PartitionCacheStats stats_;
};

}  // namespace velographx
