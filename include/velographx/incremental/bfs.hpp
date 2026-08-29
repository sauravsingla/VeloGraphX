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

    // Freeze the old BFS labels and retain deleted edges that belonged to the
    // old shortest-path DAG. Current adjacency cannot represent those removed
    // dependencies after graph mutation, but invalidation still needs them.
    const auto old_dist = dist_;
    std::vector<std::pair<VertexId, VertexId>> deleted_old_dag;
    deleted_old_dag.reserve(batch.updates.size() * (g_.directed() ? 1 : 2));
    for (const auto& e : batch.updates) {
      if (e.add || e.src >= old_dist.size() || e.dst >= old_dist.size()) continue;
      if (old_dist[e.src] != unreachable && old_dist[e.src] + 1 == old_dist[e.dst]) {
        deleted_old_dag.emplace_back(e.src, e.dst);
      }
      if (!g_.directed() && old_dist[e.dst] != unreachable && old_dist[e.dst] + 1 == old_dist[e.src]) {
        deleted_old_dag.emplace_back(e.dst, e.src);
      }
    }
    std::sort(deleted_old_dag.begin(), deleted_old_dag.end());
    deleted_old_dag.erase(std::unique(deleted_old_dag.begin(), deleted_old_dag.end()),
                          deleted_old_dag.end());

    std::vector<VertexId> deletion_candidates;
    deletion_candidates.reserve(deleted_old_dag.size());
    for (const auto& [parent, child] : deleted_old_dag) {
      (void)parent;
      deletion_candidates.push_back(child);
    }
    std::sort(deletion_candidates.begin(), deletion_candidates.end());
    deletion_candidates.erase(std::unique(deletion_candidates.begin(), deletion_candidates.end()),
                              deletion_candidates.end());
    last_deletion_candidates_ = deletion_candidates.size();

    g_.apply(batch);
    if (dist_.size() < g_.vertex_count()) dist_.resize(g_.vertex_count(), unreachable);

    if (!deletion_candidates.empty() &&
        !repair_deletions(deletion_candidates, deleted_old_dag, old_dist)) {
      recompute();
      last_used_full_recompute_ = true;
      return;
    }

    // Additions can only decrease distances. This pass is intentionally after
    // deletion repair so mixed batches converge from the repaired state.
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
  [[nodiscard]] bool has_valid_old_level_support(
      VertexId v, std::uint32_t old_level,
      const std::vector<std::uint32_t>& old_dist,
      const std::vector<std::uint8_t>& affected) const {
    if (v == source_) return old_level == 0;
    if (old_level == unreachable || old_level == 0) return false;
    for (auto p : g_.in_neighbors(v)) {
      if (p >= old_dist.size() || p >= affected.size() || affected[p]) continue;
      if (old_dist[p] != unreachable && old_dist[p] + 1 == old_level) return true;
    }
    return false;
  }

  [[nodiscard]] std::uint32_t best_boundary_distance(
      VertexId v, const std::vector<std::uint8_t>& affected) const {
    std::uint32_t best = unreachable;
    for (auto p : g_.in_neighbors(v)) {
      if (p >= dist_.size() || p >= affected.size() || affected[p] || dist_[p] == unreachable) continue;
      const auto candidate = dist_[p] + 1;
      if (candidate < best) best = candidate;
    }
    return best;
  }

  bool repair_deletions(
      const std::vector<VertexId>& candidates,
      const std::vector<std::pair<VertexId, VertexId>>& deleted_old_dag,
      const std::vector<std::uint32_t>& old_dist) {
    const auto n = g_.vertex_count();
    const auto fallback_limit = std::max<std::size_t>(
        1, static_cast<std::size_t>(static_cast<double>(n) * deletion_fallback_fraction_));

    std::vector<std::uint8_t> affected(n, 0);
    std::queue<VertexId> invalidate;

    auto invalidate_if_unsupported = [&](VertexId v) {
      if (v >= old_dist.size() || v == source_ || old_dist[v] == unreachable || affected[v]) return;
      if (has_valid_old_level_support(v, old_dist[v], old_dist, affected)) return;
      affected[v] = 1;
      dist_[v] = unreachable;
      invalidate.push(v);
      ++last_affected_vertices_;
    };

    for (auto v : candidates) invalidate_if_unsupported(v);

    while (!invalidate.empty()) {
      const auto u = invalidate.front();
      invalidate.pop();
      if (last_affected_vertices_ > fallback_limit) return false;
      if (u >= old_dist.size() || old_dist[u] == unreachable) continue;

      // Surviving old-DAG dependencies are visible in current adjacency.
      for (auto v : g_.neighbors(u)) {
        if (v < old_dist.size() && old_dist[v] == old_dist[u] + 1) {
          invalidate_if_unsupported(v);
        }
      }

      // Deleted old-DAG dependencies are no longer visible above. Revisit them
      // explicitly so a child cannot keep transient support from a parent that
      // later becomes invalid in the same batch.
      const auto first = std::lower_bound(
          deleted_old_dag.begin(), deleted_old_dag.end(),
          std::pair<VertexId, VertexId>{u, 0});
      const auto last = std::upper_bound(
          deleted_old_dag.begin(), deleted_old_dag.end(),
          std::pair<VertexId, VertexId>{u, std::numeric_limits<VertexId>::max()});
      for (auto it = first; it != last; ++it) invalidate_if_unsupported(it->second);
    }

    if (last_affected_vertices_ == 0) return true;

    // Keep every affected vertex unreachable while computing boundary seeds so
    // one not-yet-repaired affected vertex cannot become another's boundary.
    using Item = std::pair<std::uint32_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    for (VertexId v = 0; v < affected.size(); ++v) {
      if (!affected[v]) continue;
      const auto best = best_boundary_distance(v, affected);
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

    // New edges in a mixed batch can make a repaired vertex shorter than its
    // old level; propagate such decreases beyond the invalidated region.
    std::queue<VertexId> repaired_frontier;
    for (VertexId v = 0; v < affected.size(); ++v) {
      if (affected[v] && dist_[v] != unreachable &&
          (v >= old_dist.size() || dist_[v] < old_dist[v])) {
        repaired_frontier.push(v);
      }
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
