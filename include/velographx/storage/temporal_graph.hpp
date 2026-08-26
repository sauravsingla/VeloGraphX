#pragma once

#include "velographx/storage/dynamic_graph.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace velographx {

struct VersionedUpdateBatch {
  std::uint64_t version{};
  std::uint64_t min_timestamp{};
  std::uint64_t max_timestamp{};
  UpdateBatch batch;
};

class TemporalGraph {
 public:
  explicit TemporalGraph(std::size_t vertices = 0, bool directed = false)
      : initial_vertices_(vertices), directed_(directed), graph_(vertices, directed) {}

  [[nodiscard]] const DynamicGraph& graph() const noexcept { return graph_; }
  [[nodiscard]] std::uint64_t version() const noexcept { return graph_.version(); }
  [[nodiscard]] const std::vector<VersionedUpdateBatch>& history() const noexcept { return history_; }

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) return;
    std::uint64_t min_ts = batch.updates.front().timestamp;
    std::uint64_t max_ts = batch.updates.front().timestamp;
    for (const auto& update : batch.updates) {
      min_ts = std::min(min_ts, update.timestamp);
      max_ts = std::max(max_ts, update.timestamp);
    }
    graph_.apply(batch);
    history_.push_back({graph_.version(), min_ts, max_ts, batch});
  }

  [[nodiscard]] DynamicGraph snapshot_version(std::uint64_t version) const {
    if (version > graph_.version()) throw std::out_of_range("requested version is in the future");
    DynamicGraph snapshot(initial_vertices_, directed_);
    for (const auto& entry : history_) {
      if (entry.version > version) break;
      snapshot.apply(entry.batch);
    }
    return snapshot;
  }

  [[nodiscard]] DynamicGraph snapshot_time(std::uint64_t timestamp) const {
    DynamicGraph snapshot(initial_vertices_, directed_);
    for (const auto& entry : history_) {
      UpdateBatch filtered;
      for (const auto& update : entry.batch.updates) {
        if (update.timestamp <= timestamp) filtered.updates.push_back(update);
      }
      if (!filtered.empty()) snapshot.apply(filtered);
    }
    return snapshot;
  }

  [[nodiscard]] std::vector<EdgeUpdate> changes_between_versions(std::uint64_t from_exclusive,
                                                                 std::uint64_t to_inclusive) const {
    if (from_exclusive > to_inclusive || to_inclusive > graph_.version())
      throw std::out_of_range("invalid version range");
    std::vector<EdgeUpdate> out;
    for (const auto& entry : history_) {
      if (entry.version <= from_exclusive) continue;
      if (entry.version > to_inclusive) break;
      out.insert(out.end(), entry.batch.updates.begin(), entry.batch.updates.end());
    }
    return out;
  }

  [[nodiscard]] std::vector<EdgeUpdate> changes_between_times(std::uint64_t from_exclusive,
                                                              std::uint64_t to_inclusive) const {
    if (from_exclusive > to_inclusive) throw std::out_of_range("invalid timestamp range");
    std::vector<EdgeUpdate> out;
    for (const auto& entry : history_) {
      if (entry.max_timestamp <= from_exclusive || entry.min_timestamp > to_inclusive) continue;
      for (const auto& update : entry.batch.updates) {
        if (update.timestamp > from_exclusive && update.timestamp <= to_inclusive) out.push_back(update);
      }
    }
    return out;
  }

  [[nodiscard]] DynamicGraph sliding_window(std::uint64_t end_timestamp,
                                            std::uint64_t window_width) const {
    const auto begin = end_timestamp > window_width ? end_timestamp - window_width : 0;
    DynamicGraph window(initial_vertices_, directed_);
    for (const auto& entry : history_) {
      UpdateBatch filtered;
      for (const auto& update : entry.batch.updates) {
        if (update.timestamp > begin && update.timestamp <= end_timestamp) filtered.updates.push_back(update);
      }
      if (!filtered.empty()) window.apply(filtered);
    }
    return window;
  }

 private:
  std::size_t initial_vertices_{0};
  bool directed_{false};
  DynamicGraph graph_;
  std::vector<VersionedUpdateBatch> history_;
};

}  // namespace velographx
