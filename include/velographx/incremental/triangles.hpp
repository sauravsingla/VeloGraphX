#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "velographx/graph_access.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

template <class Graph>
class BasicIncrementalTriangleCount {
 public:
  explicit BasicIncrementalTriangleCount(Graph& graph) : graph_(graph) {
    validate_graph();
    recompute();
  }
  BasicIncrementalTriangleCount(Graph& graph, std::uint64_t trusted_initial_count)
      : graph_(graph), triangles_(trusted_initial_count) {
    validate_graph();
  }

  [[nodiscard]] std::uint64_t value() const noexcept { return triangles_; }

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) return;
    for (const auto& op : batch.updates) {
      if (op.src == op.dst) continue;
      const bool exists = has_edge(graph_, op.src, op.dst);
      const auto common = common_neighbors(op.src, op.dst);
      if (op.add && !exists) triangles_ += common;
      if (!op.add && exists) triangles_ -= std::min<std::uint64_t>(triangles_, common);
      UpdateBatch one;
      one.updates.push_back(op);
      apply_updates(graph_, one);
    }
  }

  void recompute() {
    std::uint64_t triple = 0;
    for (VertexId u = 0; u < vertex_count(graph_); ++u) {
      for_each_neighbor(graph_, u, [&](VertexId v) {
        if (u < v) triple += common_neighbors(u, v);
      });
    }
    triangles_ = triple / 3;
  }

 protected:
  void validate_graph() const {
    if (is_directed(graph_)) {
      throw std::invalid_argument(
          "IncrementalTriangleCount requires an undirected graph; directed motifs need an explicit definition");
    }
  }

  [[nodiscard]] std::uint64_t common_neighbors(VertexId a, VertexId b) const {
    VertexId scan = a;
    VertexId probe = b;
    if (neighbor_count(graph_, b) < neighbor_count(graph_, a)) std::swap(scan, probe);
    std::uint64_t common = 0;
    for_each_neighbor(graph_, scan, [&](VertexId v) {
      if (has_edge(graph_, probe, v)) ++common;
    });
    return common;
  }

  Graph& graph_;
  std::uint64_t triangles_{0};
};

// DynamicGraph specialization computes the sequential triangle delta against a
// lightweight in-memory edge overlay, then applies the complete batch once.
// This preserves operation-order semantics while retaining one graph version
// increment and the normal DynamicGraph storage-maintenance path.
class IncrementalTriangleCount : public BasicIncrementalTriangleCount<DynamicGraph> {
 public:
  explicit IncrementalTriangleCount(DynamicGraph& graph)
      : BasicIncrementalTriangleCount<DynamicGraph>(graph) {}
  IncrementalTriangleCount(DynamicGraph& graph, std::uint64_t trusted_initial_count)
      : BasicIncrementalTriangleCount<DynamicGraph>(graph, trusted_initial_count) {}

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) return;

    using RowOverride = std::unordered_map<VertexId, bool>;
    std::unordered_map<VertexId, RowOverride> overrides;
    overrides.reserve(batch.updates.size() * 2 + 1);

    auto effective_has_edge = [&](VertexId u, VertexId v) {
      const auto row_it = overrides.find(u);
      if (row_it != overrides.end()) {
        const auto edge_it = row_it->second.find(v);
        if (edge_it != row_it->second.end()) return edge_it->second;
      }
      return graph_.has_edge(u, v);
    };

    auto override_count = [&](VertexId u) -> std::size_t {
      const auto it = overrides.find(u);
      return it == overrides.end() ? 0 : it->second.size();
    };

    auto effective_common_neighbors = [&](VertexId a, VertexId b) {
      VertexId scan = a;
      VertexId probe = b;
      const auto estimated_a = neighbor_count(graph_, a) + override_count(a);
      const auto estimated_b = neighbor_count(graph_, b) + override_count(b);
      if (estimated_b < estimated_a) std::swap(scan, probe);

      std::unordered_set<VertexId> candidates;
      if (scan < graph_.vertex_count()) {
        graph_.for_each_neighbor(scan, [&](VertexId v) { candidates.insert(v); });
      }
      const auto row_it = overrides.find(scan);
      if (row_it != overrides.end()) {
        for (const auto& [v, present] : row_it->second) {
          (void)present;
          candidates.insert(v);
        }
      }

      std::uint64_t common = 0;
      for (const auto v : candidates) {
        if (effective_has_edge(scan, v) && effective_has_edge(probe, v)) ++common;
      }
      return common;
    };

    for (const auto& op : batch.updates) {
      if (op.src == op.dst) continue;
      const bool exists = effective_has_edge(op.src, op.dst);
      const auto common = effective_common_neighbors(op.src, op.dst);
      if (op.add && !exists) triangles_ += common;
      if (!op.add && exists) triangles_ -= std::min<std::uint64_t>(triangles_, common);

      overrides[op.src][op.dst] = op.add;
      overrides[op.dst][op.src] = op.add;
    }

    // DynamicGraph now enforces simple-graph semantics itself, so applying the
    // original batch is safe and preserves exact one-batch version semantics.
    graph_.apply(batch);
  }
};

}  // namespace velographx
