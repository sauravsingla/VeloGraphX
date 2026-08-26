#pragma once
#include <cstdint>
#include <numeric>
#include <vector>
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {
class IncrementalComponents {
 public:
  explicit IncrementalComponents(DynamicGraph& g) : g_(g) { rebuild(); }
  [[nodiscard]] std::uint32_t component(VertexId v) { return find(v); }
  void apply(const UpdateBatch& b) {
    bool deletion = false;
    for (const auto& e : b.updates) deletion |= !e.add;
    g_.apply(b);
    if (deletion) { rebuild(); return; }
    for (const auto& e : b.updates) unite(e.src, e.dst);
  }
 private:
  void rebuild() {
    parent_.resize(g_.vertex_count());
    rank_.assign(g_.vertex_count(), 0);
    std::iota(parent_.begin(), parent_.end(), 0);
    for (VertexId u = 0; u < g_.vertex_count(); ++u) for (auto v : g_.neighbors(u)) unite(u, v);
  }
  VertexId find(VertexId x) { return parent_[x] == x ? x : parent_[x] = find(parent_[x]); }
  void unite(VertexId a, VertexId b) {
    a = find(a); b = find(b); if (a == b) return;
    if (rank_[a] < rank_[b]) std::swap(a,b);
    parent_[b] = a; if (rank_[a] == rank_[b]) ++rank_[a];
  }
  DynamicGraph& g_;
  std::vector<VertexId> parent_;
  std::vector<std::uint8_t> rank_;
};
} // namespace velographx
