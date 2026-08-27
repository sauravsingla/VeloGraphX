#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <utility>
#include <vector>

namespace velographx {

using VertexId = std::uint32_t;

struct EdgeUpdate {
  VertexId src{};
  VertexId dst{};
  bool add{true};
  std::uint64_t timestamp{0};
};

struct UpdateBatch {
  std::vector<EdgeUpdate> updates;
  void add(VertexId u, VertexId v, std::uint64_t ts = 0) { updates.push_back({u, v, true, ts}); }
  void remove(VertexId u, VertexId v, std::uint64_t ts = 0) { updates.push_back({u, v, false, ts}); }
  [[nodiscard]] bool empty() const noexcept { return updates.empty(); }
};

class IncrementalTriangleCount;

class DynamicGraph {
 public:
  explicit DynamicGraph(std::size_t vertices = 0, bool directed = false)
      : directed_(directed), base_(vertices), delta_add_(vertices), delta_del_(vertices) {}

  [[nodiscard]] std::size_t vertex_count() const noexcept { return base_.size(); }
  [[nodiscard]] std::uint64_t version() const noexcept { return version_; }
  [[nodiscard]] bool directed() const noexcept { return directed_; }

  void ensure_vertex(VertexId v) {
    const auto n = static_cast<std::size_t>(v) + 1;
    if (n > base_.size()) {
      base_.resize(n);
      delta_add_.resize(n);
      delta_del_.resize(n);
    }
  }

  void add_edge(VertexId u, VertexId v) {
    UpdateBatch b;
    b.add(u, v);
    apply(b);
  }

  void remove_edge(VertexId u, VertexId v) {
    UpdateBatch b;
    b.remove(u, v);
    apply(b);
  }

  void apply(const UpdateBatch& batch) {
    for (const auto& op : batch.updates) apply_unversioned(op);
    if (!batch.empty()) ++version_;
  }

  [[nodiscard]] std::vector<VertexId> neighbors(VertexId u) const {
    if (u >= base_.size()) return {};
    std::vector<VertexId> out;
    out.reserve(base_[u].size() + delta_add_[u].size());
    for (auto v : base_[u]) if (!delta_del_[u].contains(v)) out.push_back(v);
    for (auto v : delta_add_[u]) if (!delta_del_[u].contains(v)) out.push_back(v);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }

  [[nodiscard]] bool has_edge(VertexId u, VertexId v) const {
    const auto ns = neighbors(u);
    return std::binary_search(ns.begin(), ns.end(), v);
  }

  [[nodiscard]] std::size_t edge_count_directed() const {
    std::size_t total = 0;
    for (VertexId u = 0; u < base_.size(); ++u) total += neighbors(u).size();
    return total;
  }

  [[nodiscard]] double delta_ratio() const {
    std::size_t base_edges = 0, delta = 0;
    for (std::size_t i = 0; i < base_.size(); ++i) {
      base_edges += base_[i].size();
      delta += delta_add_[i].size() + delta_del_[i].size();
    }
    return static_cast<double>(delta) / static_cast<double>(std::max<std::size_t>(1, base_edges));
  }

  bool maybe_compact(double threshold = 0.25) {
    if (delta_ratio() < threshold) return false;
    compact();
    return true;
  }

  void compact() {
    for (VertexId u = 0; u < base_.size(); ++u) {
      auto merged = neighbors(u);
      base_[u] = std::move(merged);
      delta_add_[u].clear();
      delta_del_[u].clear();
    }
  }

 private:
  friend class IncrementalTriangleCount;

  void apply_unversioned(const EdgeUpdate& op) {
    ensure_vertex(std::max(op.src, op.dst));
    apply_one(op.src, op.dst, op.add);
    if (!directed_ && op.src != op.dst) apply_one(op.dst, op.src, op.add);
  }

  void apply_one(VertexId u, VertexId v, bool add) {
    if (add) {
      delta_del_[u].erase(v);
      if (!std::binary_search(base_[u].begin(), base_[u].end(), v)) delta_add_[u].insert(v);
    } else {
      delta_add_[u].erase(v);
      if (std::binary_search(base_[u].begin(), base_[u].end(), v)) delta_del_[u].insert(v);
    }
  }

  bool directed_{false};
  std::vector<std::vector<VertexId>> base_;
  std::vector<std::unordered_set<VertexId>> delta_add_;
  std::vector<std::unordered_set<VertexId>> delta_del_;
  std::uint64_t version_{0};
};

}  // namespace velographx
