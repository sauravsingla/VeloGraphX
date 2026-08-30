#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_set>
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

    ensure_workspace(g_.vertex_count());
    prepare_batch_workspace(batch.updates.size());

    for (auto it = batch.updates.rbegin(); it != batch.updates.rend(); ++it) {
      auto u = it->src;
      auto v = it->dst;
      if (!g_.directed() && v < u) std::swap(u, v);
      const auto key = edge_key(u, v);
      if (!seen_final_updates_.insert(key).second) continue;
      if (it->add) {
        final_additions_.emplace_back(it->src, it->dst);
      } else {
        final_deletions_.emplace_back(it->src, it->dst);
        final_deletion_keys_.insert(key);
      }
    }

    deletion_candidates_.reserve(final_deletions_.size() * (g_.directed() ? 1 : 2));
    for (const auto& [u, v] : final_deletions_) {
      if (!g_.has_edge(u, v)) continue;
      if (is_shortest_parent(u, v)) deletion_candidates_.push_back(v);
      if (!g_.directed() && is_shortest_parent(v, u)) deletion_candidates_.push_back(u);
    }
    std::sort(deletion_candidates_.begin(), deletion_candidates_.end());
    deletion_candidates_.erase(std::unique(deletion_candidates_.begin(), deletion_candidates_.end()),
                               deletion_candidates_.end());
    last_deletion_candidates_ = deletion_candidates_.size();

    affected_vertices_.reserve(std::min<std::size_t>(deletion_candidates_.size() * 2 + 8,
                                                      g_.vertex_count()));
    bool fallback_needed = false;
    if (!deletion_candidates_.empty()) {
      fallback_needed = !compute_affected_prebatch(final_deletions_, final_deletion_keys_);
    }

    g_.apply(batch);
    if (dist_.size() < g_.vertex_count()) dist_.resize(g_.vertex_count(), unreachable);
    ensure_workspace(g_.vertex_count());

    if (fallback_needed) {
      clear_workspace();
      recompute();
      last_used_full_recompute_ = true;
      return;
    }

    if (!affected_vertices_.empty()) {
      old_affected_dist_.reserve(affected_vertices_.size());
      for (auto v : affected_vertices_) {
        old_affected_dist_.push_back(v < dist_.size() ? dist_[v] : unreachable);
      }
      repair_affected();
    }

    bfs_queue_.clear();
    for (const auto& [u, v] : final_additions_) {
      relax_edge(u, v, bfs_queue_);
      if (!g_.directed()) relax_edge(v, u, bfs_queue_);
    }
    propagate_decreases(bfs_queue_);
    clear_workspace();
  }

  void recompute() {
    dist_.assign(g_.vertex_count(), unreachable);
    ensure_workspace(g_.vertex_count());
    if (source_ >= g_.vertex_count()) return;
    bfs_queue_.clear();
    bfs_queue_.reserve(std::max(bfs_queue_.capacity(), g_.vertex_count()));
    dist_[source_] = 0;
    bfs_queue_.push_back(source_);
    std::size_t head = 0;
    while (head < bfs_queue_.size()) {
      const auto u = bfs_queue_[head++];
      for (auto v : g_.neighbors(u)) {
        if (dist_[v] == unreachable) {
          dist_[v] = dist_[u] + 1;
          bfs_queue_.push_back(v);
        }
      }
    }
  }

 private:
  [[nodiscard]] std::uint64_t edge_key(VertexId u, VertexId v) const noexcept {
    if (!g_.directed() && v < u) std::swap(u, v);
    return (static_cast<std::uint64_t>(u) << 32) | static_cast<std::uint64_t>(v);
  }

  void ensure_workspace(std::size_t vertices) {
    if (affected_.size() < vertices) affected_.resize(vertices, 0);
    if (lost_parent_count_.size() < vertices) lost_parent_count_.resize(vertices, 0);
    if (shortest_parent_count_.size() < vertices) shortest_parent_count_.resize(vertices, 0);
  }

  void prepare_batch_workspace(std::size_t operations) {
    seen_final_updates_.clear();
    final_deletion_keys_.clear();
    final_additions_.clear();
    final_deletions_.clear();
    deletion_candidates_.clear();
    affected_vertices_.clear();
    old_affected_dist_.clear();
    invalidate_.clear();
    touched_loss_vertices_.clear();

    const auto hash_capacity = operations * 2 + 1;
    if (seen_final_updates_.bucket_count() < hash_capacity) seen_final_updates_.reserve(hash_capacity);
    if (final_deletion_keys_.bucket_count() < operations + 1) final_deletion_keys_.reserve(operations + 1);
    if (final_additions_.capacity() < operations) final_additions_.reserve(operations);
    if (final_deletions_.capacity() < operations) final_deletions_.reserve(operations);
  }

  [[nodiscard]] bool is_shortest_parent(VertexId u, VertexId v) const noexcept {
    return u < dist_.size() && v < dist_.size() && dist_[u] != unreachable &&
           dist_[v] != unreachable && dist_[u] + 1 == dist_[v];
  }

  [[nodiscard]] std::uint32_t shortest_parent_count(VertexId v) {
    if (v >= dist_.size() || v == source_ || dist_[v] == unreachable) return 0;
    if (shortest_parent_count_[v] != 0) return shortest_parent_count_[v];
    std::uint32_t count = 0;
    for (auto p : g_.in_neighbors(v)) {
      if (is_shortest_parent(p, v)) ++count;
    }
    shortest_parent_count_[v] = count;
    return count;
  }

  bool compute_affected_prebatch(
      const std::vector<std::pair<VertexId, VertexId>>& final_deletions,
      const std::unordered_set<std::uint64_t>& final_deletion_keys) {
    const auto fallback_limit = std::max<std::size_t>(
        1, static_cast<std::size_t>(static_cast<double>(g_.vertex_count()) * deletion_fallback_fraction_));
    invalidate_.reserve(std::min<std::size_t>(final_deletions.size() * 2 + 8, g_.vertex_count()));

    auto record_parent_loss = [&](VertexId v) {
      if (v >= dist_.size() || v == source_ || dist_[v] == unreachable || affected_[v]) return;
      if (lost_parent_count_[v] == 0) touched_loss_vertices_.push_back(v);
      ++lost_parent_count_[v];
      const auto support = shortest_parent_count(v);
      if (support != 0 && lost_parent_count_[v] >= support) {
        affected_[v] = 1;
        affected_vertices_.push_back(v);
        invalidate_.push_back(v);
        ++last_affected_vertices_;
      }
    };

    for (const auto& [u, v] : final_deletions) {
      if (!g_.has_edge(u, v)) continue;
      if (is_shortest_parent(u, v)) record_parent_loss(v);
      if (!g_.directed() && is_shortest_parent(v, u)) record_parent_loss(u);
    }

    std::size_t head = 0;
    while (head < invalidate_.size()) {
      if (last_affected_vertices_ > fallback_limit) return false;
      const auto u = invalidate_[head++];
      if (u >= dist_.size() || dist_[u] == unreachable) continue;
      for (auto v : g_.neighbors(u)) {
        if (v >= dist_.size() || dist_[v] != dist_[u] + 1) continue;
        if (final_deletion_keys.contains(edge_key(u, v))) continue;
        record_parent_loss(v);
      }
    }
    return last_affected_vertices_ <= fallback_limit;
  }

  [[nodiscard]] std::uint32_t best_boundary_distance(VertexId v) const {
    std::uint32_t best = unreachable;
    for (auto p : g_.in_neighbors(v)) {
      if (p >= dist_.size() || p >= affected_.size() || affected_[p] || dist_[p] == unreachable) continue;
      const auto candidate = dist_[p] + 1;
      if (candidate < best) best = candidate;
    }
    return best;
  }

  void repair_affected() {
    for (auto v : affected_vertices_) {
      if (v < dist_.size()) dist_[v] = unreachable;
    }

    using Item = std::pair<std::uint32_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    for (auto v : affected_vertices_) {
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
        if (v >= affected_.size() || !affected_[v]) continue;
        const auto candidate = du + 1;
        if (candidate < dist_[v]) {
          dist_[v] = candidate;
          pq.emplace(candidate, v);
        }
      }
    }

    bfs_queue_.clear();
    for (std::size_t i = 0; i < affected_vertices_.size(); ++i) {
      const auto v = affected_vertices_[i];
      const auto old_dist = i < old_affected_dist_.size() ? old_affected_dist_[i] : unreachable;
      if (v < dist_.size() && dist_[v] != unreachable && dist_[v] < old_dist) {
        bfs_queue_.push_back(v);
      }
    }
    propagate_decreases(bfs_queue_);
  }

  void clear_workspace() {
    for (auto v : affected_vertices_) {
      if (v < affected_.size()) affected_[v] = 0;
    }
    for (auto v : touched_loss_vertices_) {
      if (v < lost_parent_count_.size()) lost_parent_count_[v] = 0;
      if (v < shortest_parent_count_.size()) shortest_parent_count_[v] = 0;
    }
    touched_loss_vertices_.clear();
  }

  void relax_edge(VertexId u, VertexId v, std::vector<VertexId>& q) {
    if (u >= dist_.size() || v >= dist_.size() || dist_[u] == unreachable) return;
    if (dist_[u] + 1 < dist_[v]) {
      dist_[v] = dist_[u] + 1;
      q.push_back(v);
    }
  }

  void propagate_decreases(std::vector<VertexId>& q) {
    std::size_t head = 0;
    while (head < q.size()) {
      const auto u = q[head++];
      if (dist_[u] == unreachable) continue;
      for (auto v : g_.neighbors(u)) {
        if (dist_[u] + 1 < dist_[v]) {
          dist_[v] = dist_[u] + 1;
          q.push_back(v);
        }
      }
    }
  }

  DynamicGraph& g_;
  VertexId source_;
  double deletion_fallback_fraction_{0.35};
  std::vector<std::uint32_t> dist_;
  std::vector<std::uint8_t> affected_;
  std::vector<std::uint32_t> lost_parent_count_;
  std::vector<std::uint32_t> shortest_parent_count_;
  std::vector<VertexId> touched_loss_vertices_;

  std::unordered_set<std::uint64_t> seen_final_updates_;
  std::unordered_set<std::uint64_t> final_deletion_keys_;
  std::vector<std::pair<VertexId, VertexId>> final_additions_;
  std::vector<std::pair<VertexId, VertexId>> final_deletions_;
  std::vector<VertexId> deletion_candidates_;
  std::vector<VertexId> affected_vertices_;
  std::vector<std::uint32_t> old_affected_dist_;
  std::vector<VertexId> invalidate_;
  std::vector<VertexId> bfs_queue_;

  std::size_t last_deletion_candidates_{0};
  std::size_t last_affected_vertices_{0};
  bool last_used_full_recompute_{false};
};
} // namespace velographx
