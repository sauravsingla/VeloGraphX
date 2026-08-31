#pragma once

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <unordered_set>
#include <vector>

#include "velographx/graph_access.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

template <class Graph>
class BasicIncrementalComponents {
 public:
  explicit BasicIncrementalComponents(Graph& g) : g_(g) { rebuild(); }

  [[nodiscard]] std::uint32_t component(VertexId v) { return find(v); }
  [[nodiscard]] std::size_t last_repaired_vertices() const noexcept {
    return last_repaired_vertices_;
  }

  void apply(const UpdateBatch& batch) {
    std::unordered_set<VertexId> affected_roots;
    affected_roots.reserve(batch.updates.size() * 2);

    for (const auto& e : batch.updates) {
      if (!e.add && e.src < parent_.size() && e.dst < parent_.size()) {
        affected_roots.insert(find(e.src));
        affected_roots.insert(find(e.dst));
      }
    }

    std::vector<std::uint8_t> affected(parent_.size(), 0);
    last_repaired_vertices_ = 0;
    if (!affected_roots.empty()) {
      for (VertexId v = 0; v < parent_.size(); ++v) {
        if (affected_roots.contains(find(v))) {
          affected[v] = 1;
          ++last_repaired_vertices_;
        }
      }
    }

    apply_updates(g_, batch);
    ensure_capacity();

    if (!affected_roots.empty()) {
      for (VertexId v = 0; v < affected.size(); ++v) {
        if (affected[v]) {
          parent_[v] = v;
          rank_[v] = 0;
        }
      }

      for (VertexId u = 0; u < affected.size(); ++u) {
        if (!affected[u]) continue;
        for_each_neighbor(g_, u, [&](VertexId v) {
          if (v < affected.size() && affected[v]) unite(u, v);
        });
      }
    }

    for (const auto& e : batch.updates) {
      if (e.add) unite(e.src, e.dst);
    }
  }

 private:
  void ensure_capacity() {
    const auto old_size = parent_.size();
    if (old_size >= vertex_count(g_)) return;
    parent_.resize(vertex_count(g_));
    rank_.resize(vertex_count(g_), 0);
    for (VertexId v = static_cast<VertexId>(old_size); v < vertex_count(g_); ++v) parent_[v] = v;
  }

  void rebuild() {
    parent_.resize(vertex_count(g_));
    rank_.assign(vertex_count(g_), 0);
    std::iota(parent_.begin(), parent_.end(), 0);
    for (VertexId u = 0; u < vertex_count(g_); ++u) {
      for_each_neighbor(g_, u, [&](VertexId v) { unite(u, v); });
    }
    last_repaired_vertices_ = vertex_count(g_);
  }

  VertexId find(VertexId x) { return parent_[x] == x ? x : parent_[x] = find(parent_[x]); }

  void unite(VertexId a, VertexId b) {
    ensure_capacity();
    a = find(a);
    b = find(b);
    if (a == b) return;
    if (rank_[a] < rank_[b]) std::swap(a, b);
    parent_[b] = a;
    if (rank_[a] == rank_[b]) ++rank_[a];
  }

  Graph& g_;
  std::vector<VertexId> parent_;
  std::vector<std::uint8_t> rank_;
  std::size_t last_repaired_vertices_{0};
};

using IncrementalComponents = BasicIncrementalComponents<DynamicGraph>;

}  // namespace velographx
