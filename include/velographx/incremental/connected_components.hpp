#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "velographx/graph_access.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

template <class Graph>
class BasicIncrementalComponents {
 public:
  explicit BasicIncrementalComponents(Graph& g) : g_(g) {
    if (is_directed(g_)) {
      throw std::invalid_argument(
          "IncrementalComponents requires an undirected graph; use an explicitly defined weak/strong connectivity algorithm for directed graphs");
    }
    rebuild();
  }

  [[nodiscard]] std::uint32_t component(VertexId v) {
    if (v >= parent_.size()) throw std::out_of_range("vertex id outside graph");
    return find(v);
  }
  [[nodiscard]] std::size_t last_repaired_vertices() const noexcept {
    return last_repaired_vertices_;
  }

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) {
      last_repaired_vertices_ = 0;
      return;
    }

    const auto canonical = canonicalize(batch);
    bool effective_deletion = false;
    for (const auto& e : canonical.updates) {
      if (!e.add && has_edge(g_, e.src, e.dst)) {
        effective_deletion = true;
        break;
      }
    }

    apply_updates(g_, batch);
    ensure_capacity();

    // Deletions can split arbitrary parts of an existing component. Prefer an
    // exact full rebuild until a formally bounded deletion-repair algorithm is
    // used. This also makes add/remove conflicts within one batch exact.
    if (effective_deletion) {
      rebuild();
      return;
    }

    last_repaired_vertices_ = 0;
    std::vector<std::uint8_t> touched(parent_.size(), 0);
    for (const auto& e : canonical.updates) {
      if (!e.add || e.src >= parent_.size() || e.dst >= parent_.size()) continue;
      if (!has_edge(g_, e.src, e.dst)) continue;
      if (!touched[e.src]) {
        touched[e.src] = 1;
        ++last_repaired_vertices_;
      }
      if (!touched[e.dst]) {
        touched[e.dst] = 1;
        ++last_repaired_vertices_;
      }
      unite(e.src, e.dst);
    }
  }

 private:
  static std::uint64_t edge_key(VertexId u, VertexId v) noexcept {
    if (v < u) std::swap(u, v);
    return (static_cast<std::uint64_t>(u) << 32U) | static_cast<std::uint64_t>(v);
  }

  static UpdateBatch canonicalize(const UpdateBatch& batch) {
    UpdateBatch out;
    out.updates.reserve(batch.updates.size());
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(batch.updates.size() * 2 + 1);

    for (auto it = batch.updates.rbegin(); it != batch.updates.rend(); ++it) {
      if (it->src == it->dst) continue;
      auto op = *it;
      if (op.dst < op.src) std::swap(op.src, op.dst);
      if (seen.insert(edge_key(op.src, op.dst)).second) out.updates.push_back(op);
    }
    std::reverse(out.updates.begin(), out.updates.end());
    return out;
  }

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
