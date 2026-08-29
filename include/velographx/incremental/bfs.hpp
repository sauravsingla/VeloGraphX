#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {
class IncrementalBFS {
 public:
  static constexpr std::uint32_t unreachable = std::numeric_limits<std::uint32_t>::max();

  IncrementalBFS(DynamicGraph& g, VertexId source, double deletion_fallback_fraction = 0.35)
      : g_(g), source_(source), deletion_fallback_fraction_(deletion_fallback_fraction) {
    recompute();
  }

  [[nodiscard]] const std::vector<std::uint32_t>& distances() const noexcept { return dist_; }
  [[nodiscard]] std::size_t last_deletion_candidates() const noexcept { return last_deletion_candidates_; }
  [[nodiscard]] std::size_t last_affected_vertices() const noexcept { return last_affected_vertices_; }
  [[nodiscard]] bool last_used_full_recompute() const noexcept { return last_used_full_recompute_; }

  void apply(const UpdateBatch& batch) {
    last_deletion_candidates_ = 0;
    last_affected_vertices_ = 0;
    last_used_full_recompute_ = false;
    if (batch.empty()) return;

    // Record only deletions that can invalidate the current shortest-path DAG.
    // The support test after mutation filters deletions that still have an
    // alternate predecessor at the same BFS level.
    std::vector<VertexId> deletion_candidates;
    deletion_candidates.reserve(batch.updates.size());
    for (const auto& e : batch.updates) {
      if (e.add || e.src >= dist_.size() || e.dst >= dist_.size()) continue;
      if (dist_[e.src] != unreachable && dist_[e.src] + 1 == dist_[e.dst]) {
        deletion_candidates.push_back(e.dst);
      }
      if (!g_.directed() && dist_[e.dst] != unreachable && dist_[e.dst] + 1 == dist_[e.src]) {
        deletion_candidates.push_back(e.src);
      }
    }
    std::sort(deletion_candidates.begin(), deletion_candidates.end());
    deletion_candidates.erase(std::unique(deletion_candidates.begin(), deletion_candidates.end()),
                              deletion_candidates.end());
    last_deletion_candidates_ = deletion_candidates.size();

    g_.apply(batch);
    if (dist_.size() < g_.vertex_count()) dist_.resize(g_.vertex_count(), unreachable);

    if (!deletion_candidates.empty() && !repair_deletions(deletion_candidates)) {
      recompute();
      last_used_full_recompute_ = true;
      return;
    }

    // Additions can only decrease distances. Run the normal incremental
    // relaxation after deletion repair so mixed batches are handled exactly.
    std::queue<VertexId> q;
    for (const auto& e : batch.updates) {
      if (!e.add) continue;
      relax_edge(e.src, e.dst, q);
      if (!g_.directed()) relax_edge(e.dst, e.src, q);
    }
    propagate_decreases(q);
  }

  void recompute() {
    dist_.assign(g_.vertex_count(), unreachable);
    if (source_ >= g_.vertex_count()) return;
    std::queue<VertexId> q;
    dist_[source_] = 0;
    q.push(source_);
    while (!q.empty()) {
      const auto u = q.front();
      q.pop();
      for (auto v : g_.neighbors(u)) {
        if (dist_[v] == unreachable) {
          dist_[v] = dist_[u] + 1;
          q.push(v);
        }
      }
    }
  }

 private:
  [[nodiscard]] bool has_level_support(VertexId v, std::uint32_t level) const {
    if (v == source_) return level == 0;
    if (level == unreachable || level == 0) return false;
    for (auto p : g_.in_neighbors(v)) {
      if (p < dist_.size() && dist_[p] != unreachable && dist_[p] + 1 == level) return true;
    }
    return false;
  }

  [[nodiscard]] std::uint32_t best_boundary_distance(VertexId v) const {
    std::uint32_t best = unreachable;
    for (auto p : g_.in_neighbors(v)) {
      if (p >= dist_.size() || dist_[p] == unreachable) continue;
      const auto candidate = dist_[p] + 1;
      if (candidate < best) best = candidate;
    }
    return best;
  }

  bool repair_deletions(const std::vector<VertexId>& candidates) {
    const auto n = g_.vertex_count();
    const auto fallback_limit = std::max<std::size_t>(
        1, static_cast<std::size_t>(static_cast<double>(n) * deletion_fallback_fraction_));

    std::vector<std::uint8_t> affected(n, 0);
    std::queue<std::pair<VertexId, std::uint32_t>> invalidate;

    auto invalidate_if_unsupported = [&](VertexId v) {
      if (v >= dist_.size() || v == source_ || dist_[v] == unreachable || affected[v]) return;
      const auto old_level = dist_[v];
      if (has_level_support(v, old_level)) return;
      affected[v] = 1;
      dist_[v] = unreachable;
      invalidate.emplace(v, old_level);
      ++last_affected_vertices_;
    };

    for (auto v : candidates) invalidate_if_unsupported(v);

    // Invalidate descendants whose last predecessor at the previous level was
    // removed. Alternate shortest-path parents keep a vertex valid.
    while (!invalidate.empty()) {
      const auto [u, old_level] = invalidate.front();
      invalidate.pop();
      if (last_affected_vertices_ > fallback_limit) return false;
      for (auto v : g_.neighbors(u)) {
        if (v < dist_.size() && dist_[v] == old_level + 1) invalidate_if_unsupported(v);
      }
    }

    if (last_affected_vertices_ == 0) return true;

    // All invalid vertices are now unreachable. Re-seed them only from
    // surviving boundary predecessors, then rebuild shortest distances inside
    // the affected region in increasing-distance order.
    using Item = std::pair<std::uint32_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    for (VertexId v = 0; v < affected.size(); ++v) {
      if (!affected[v]) continue;
      const auto best = best_boundary_distance(v);
      if (best != unreachable) {
        dist_[v] = best;
        pq.emplace(best, v);
      }
    }

    while (!pq.empty()) {
      const auto [du, u] = pq.top();
      pq.pop();
      if (u >= dist_.size() || du != dist_[u]) continue;
      for (auto v : g_.neighbors(u)) {
        if (v >= affected.size() || !affected[v]) continue;
        const auto candidate = du + 1;
        if (candidate < dist_[v]) {
          dist_[v] = candidate;
          pq.emplace(candidate, v);
        }
      }
    }

    // In a mixed batch, additions are already present while the deletion region
    // is repaired. A repaired vertex can therefore emerge at a shorter level
    // than before the batch. Propagate such decreases beyond the invalidated
    // region before processing the explicit insertion-edge frontier below.
    std::queue<VertexId> repaired_frontier;
    for (VertexId v = 0; v < affected.size(); ++v) {
      if (affected[v] && dist_[v] != unreachable) repaired_frontier.push(v);
    }
    propagate_decreases(repaired_frontier);
    return true;
  }

  void relax_edge(VertexId u, VertexId v, std::queue<VertexId>& q) {
    if (u >= dist_.size() || v >= dist_.size() || dist_[u] == unreachable) return;
    if (dist_[u] + 1 < dist_[v]) {
      dist_[v] = dist_[u] + 1;
      q.push(v);
    }
  }

  void propagate_decreases(std::queue<VertexId>& q) {
    while (!q.empty()) {
      const auto u = q.front();
      q.pop();
      if (dist_[u] == unreachable) continue;
      for (auto v : g_.neighbors(u)) {
        if (dist_[u] + 1 < dist_[v]) {
          dist_[v] = dist_[u] + 1;
          q.push(v);
        }
      }
    }
  }

  DynamicGraph& g_;
  VertexId source_;
  double deletion_fallback_fraction_{0.35};
  std::vector<std::uint32_t> dist_;
  std::size_t last_deletion_candidates_{0};
  std::size_t last_affected_vertices_{0};
  bool last_used_full_recompute_{false};
};
} // namespace velographx
