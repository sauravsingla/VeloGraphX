#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

class IncrementalKCore {
 public:
  explicit IncrementalKCore(DynamicGraph& g) : g_(g) { recompute(); }

  [[nodiscard]] const std::vector<std::uint32_t>& core() const noexcept {
    return core_;
  }

  [[nodiscard]] std::size_t last_repaired_vertices() const noexcept {
    return last_repaired_vertices_;
  }

  void apply(const UpdateBatch& batch) {
    if (batch.updates.empty()) {
      last_repaired_vertices_ = 0;
      return;
    }

    // The current k-core definition is based on the graph's adjacency lists.
    // For directed graphs, component-scoped repair is not sufficient to bound
    // all dependency effects, so retain the safe full recomputation fallback.
    if (g_.directed()) {
      g_.apply(batch);
      recompute();
      return;
    }

    // Capture all pre-update components touched by update endpoints. Deletions
    // can split these components, while insertions can later merge them.
    std::vector<std::uint8_t> affected(g_.vertex_count(), 0);
    for (const auto& e : batch.updates) {
      if (e.src < g_.vertex_count()) mark_component(e.src, affected);
      if (e.dst < g_.vertex_count()) mark_component(e.dst, affected);
    }

    g_.apply(batch);
    if (core_.size() < g_.vertex_count()) core_.resize(g_.vertex_count(), 0);
    affected.resize(g_.vertex_count(), 0);

    // Capture post-update components as well. This includes any components
    // merged by insertions and all pieces created by deletions.
    for (const auto& e : batch.updates) {
      if (e.src < g_.vertex_count()) mark_component(e.src, affected);
      if (e.dst < g_.vertex_count()) mark_component(e.dst, affected);
    }

    recompute_region(affected);
  }

  void recompute() {
    const auto n = g_.vertex_count();
    core_.assign(n, 0);
    std::vector<std::uint8_t> all(n, 1);
    recompute_region(all);
  }

 private:
  void mark_component(VertexId seed, std::vector<std::uint8_t>& marked) const {
    if (seed >= g_.vertex_count() || seed >= marked.size() || marked[seed]) return;
    std::queue<VertexId> q;
    marked[seed] = 1;
    q.push(seed);
    while (!q.empty()) {
      const auto u = q.front();
      q.pop();
      for (auto v : g_.neighbors(u)) {
        if (v < marked.size() && !marked[v]) {
          marked[v] = 1;
          q.push(v);
        }
      }
    }
  }

  void recompute_region(const std::vector<std::uint8_t>& affected) {
    const auto n = g_.vertex_count();
    if (core_.size() < n) core_.resize(n, 0);

    std::vector<std::uint32_t> degree(n, 0);
    std::uint32_t max_degree = 0;
    last_repaired_vertices_ = 0;

    for (VertexId u = 0; u < n; ++u) {
      if (u >= affected.size() || !affected[u]) continue;
      ++last_repaired_vertices_;
      for (auto v : g_.neighbors(u)) {
        if (v < affected.size() && affected[v]) ++degree[u];
      }
      max_degree = std::max(max_degree, degree[u]);
      core_[u] = 0;
    }

    if (last_repaired_vertices_ == 0) return;

    std::vector<std::vector<VertexId>> bins(max_degree + 1);
    for (VertexId u = 0; u < n; ++u) {
      if (u < affected.size() && affected[u]) bins[degree[u]].push_back(u);
    }

    std::vector<std::uint8_t> removed(n, 0);
    for (std::uint32_t k = 0; k <= max_degree; ++k) {
      std::queue<VertexId> q;
      for (auto u : bins[k]) {
        if (!removed[u] && degree[u] <= k) q.push(u);
      }
      while (!q.empty()) {
        const auto u = q.front();
        q.pop();
        if (removed[u]) continue;
        removed[u] = 1;
        core_[u] = k;
        for (auto v : g_.neighbors(u)) {
          if (v >= affected.size() || !affected[v] || removed[v]) continue;
          if (degree[v] > k) {
            --degree[v];
            if (degree[v] <= k) q.push(v);
          }
        }
      }
    }
  }

  DynamicGraph& g_;
  std::vector<std::uint32_t> core_;
  std::size_t last_repaired_vertices_{0};
};

}  // namespace velographx
