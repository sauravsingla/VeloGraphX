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

    const auto old_dist = dist_;

    // Dependency invalidation must be derived from the pre-batch graph. Once
    // deletions are applied, removed shortest-path edges disappear from
    // adjacency and their downstream causal links can no longer be recovered.
    std::vector<std::pair<VertexId, VertexId>> deleted_edges;
    std::vector<VertexId> deletion_candidates;
    deleted_edges.reserve(batch.updates.size() * (g_.directed() ? 1 : 2));
    deletion_candidates.reserve(batch.updates.size() * (g_.directed() ? 1 : 2));

    for (const auto& e : batch.updates) {
      if (e.add) continue;
      deleted_edges.emplace_back(e.src, e.dst);
      if (!g_.directed()) deleted_edges.emplace_back(e.dst, e.src);

      if (e.src < old_dist.size() && e.dst < old_dist.size() &&
          old_dist[e.src] != unreachable && old_dist[e.src] + 1 == old_dist[e.dst]) {
        deletion_candidates.push_back(e.dst);
      }
      if (!g_.directed() && e.src < old_dist.size() && e.dst < old_dist.size() &&
          old_dist[e.dst] != unreachable && old_dist[e.dst] + 1 == old_dist[e.src]) {
        deletion_candidates.push_back(e.src);
      }
    }

    std::sort(deleted_edges.begin(), deleted_edges.end());
    deleted_edges.erase(std::unique(deleted_edges.begin(), deleted_edges.end()), deleted_edges.end());
    std::sort(deletion_candidates.begin(), deletion_candidates.end());
    deletion_candidates.erase(std::unique(deletion_candidates.begin(), deletion_candidates.end()),
                              deletion_candidates.end());
    last_deletion_candidates_ = deletion_candidates.size();

    std::vector<std::uint8_t> affected(g_.vertex_count(), 0);
    std::vector<VertexId> affected_vertices;
    affected_vertices.reserve(std::min<std::size_t>(deletion_candidates.size() * 4 + 8,
                                                     g_.vertex_count()));
    bool fallback_needed = false;
    if (!deletion_candidates.empty()) {
      fallback_needed = !compute_affected_prebatch(
          deletion_candidates, deleted_edges, old_dist, affected, affected_vertices);
    }

    g_.apply(batch);
    if (dist_.size() < g_.vertex_count()) dist_.resize(g_.vertex_count(), unreachable);
    if (affected.size() < g_.vertex_count()) affected.resize(g_.vertex_count(), 0);

    if (fallback_needed) {
      recompute();
      last_used_full_recompute_ = true;
      return;
    }

    if (!affected_vertices.empty()) repair_affected(affected, affected_vertices, old_dist);

    // Additions can only decrease distances, but only the last update for a
    // logical edge describes its final batch state. Processing an earlier add
    // that is later removed would relax through a non-existent final edge.
    std::unordered_set<std::uint64_t> seen_final_updates;
    seen_final_updates.reserve(batch.updates.size() * 2 + 1);
    std::vector<std::pair<VertexId, VertexId>> final_additions;
    final_additions.reserve(batch.updates.size());
    for (auto it = batch.updates.rbegin(); it != batch.updates.rend(); ++it) {
      auto u = it->src;
      auto v = it->dst;
      if (!g_.directed() && v < u) std::swap(u, v);
      const auto key = (static_cast<std::uint64_t>(u) << 32) | static_cast<std::uint64_t>(v);
      if (!seen_final_updates.insert(key).second || !it->add) continue;
      final_additions.emplace_back(it->src, it->dst);
    }

    std::queue<VertexId> q;
    for (const auto& [u, v] : final_additions) {
      relax_edge(u, v, q);
      if (!g_.directed()) relax_edge(v, u, q);
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
  [[nodiscard]] bool edge_deleted(
      VertexId u, VertexId v,
      const std::vector<std::pair<VertexId, VertexId>>& deleted_edges) const {
    return std::binary_search(deleted_edges.begin(), deleted_edges.end(),
                              std::pair<VertexId, VertexId>{u, v});
  }

  [[nodiscard]] bool has_surviving_old_support(
      VertexId v,
      const std::vector<std::pair<VertexId, VertexId>>& deleted_edges,
      const std::vector<std::uint32_t>& old_dist,
      const std::vector<std::uint8_t>& affected) const {
    if (v >= old_dist.size() || v >= affected.size() || v == source_ ||
        old_dist[v] == unreachable) {
      return false;
    }
    for (auto p : g_.in_neighbors(v)) {
      if (p >= old_dist.size() || p >= affected.size() || affected[p]) continue;
      if (old_dist[p] == unreachable || old_dist[p] + 1 != old_dist[v]) continue;
      if (!edge_deleted(p, v, deleted_edges)) return true;
    }
    return false;
  }

  bool compute_affected_prebatch(
      const std::vector<VertexId>& candidates,
      const std::vector<std::pair<VertexId, VertexId>>& deleted_edges,
      const std::vector<std::uint32_t>& old_dist,
      std::vector<std::uint8_t>& affected,
      std::vector<VertexId>& affected_vertices) {
    const auto fallback_limit = std::max<std::size_t>(
        1, static_cast<std::size_t>(static_cast<double>(affected.size()) * deletion_fallback_fraction_));
    std::queue<VertexId> invalidate;

    // Support is discovered lazily only for vertices reached from deleted
    // shortest-path edges. Rechecking a child whenever one of its old-DAG
    // parents becomes invalidated drives the dependency loss to a fixed point
    // without an O(E) support-count scan on every deletion batch.
    auto try_invalidate = [&](VertexId v) {
      if (v >= affected.size() || v >= old_dist.size() || v == source_ ||
          old_dist[v] == unreachable || affected[v]) {
        return;
      }
      if (has_surviving_old_support(v, deleted_edges, old_dist, affected)) return;
      affected[v] = 1;
      affected_vertices.push_back(v);
      invalidate.push(v);
      ++last_affected_vertices_;
    };

    for (auto v : candidates) try_invalidate(v);

    while (!invalidate.empty()) {
      if (last_affected_vertices_ > fallback_limit) return false;
      const auto u = invalidate.front();
      invalidate.pop();
      if (u >= old_dist.size() || old_dist[u] == unreachable) continue;

      // Pre-mutation adjacency contains every old shortest-path child. A child
      // that retained another support when first examined is reconsidered when
      // that alternate parent is later invalidated, so multi-parent deletions
      // remain order-independent.
      for (auto v : g_.neighbors(u)) {
        if (v < old_dist.size() && old_dist[v] == old_dist[u] + 1) {
          try_invalidate(v);
        }
      }
    }
    return last_affected_vertices_ <= fallback_limit;
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

  void repair_affected(const std::vector<std::uint8_t>& affected,
                       const std::vector<VertexId>& affected_vertices,
                       const std::vector<std::uint32_t>& old_dist) {
    for (auto v : affected_vertices) {
      if (v < dist_.size()) dist_[v] = unreachable;
    }

    using Item = std::pair<std::uint32_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    for (auto v : affected_vertices) {
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

    // A mixed batch may introduce a new edge that makes a repaired vertex
    // shorter than its pre-batch level. Propagate that decrease outside the
    // invalidated region before processing surviving additions below.
    std::queue<VertexId> repaired_frontier;
    for (auto v : affected_vertices) {
      if (v < dist_.size() && dist_[v] != unreachable &&
          (v >= old_dist.size() || dist_[v] < old_dist[v])) {
        repaired_frontier.push(v);
      }
    }
    propagate_decreases(repaired_frontier);
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
