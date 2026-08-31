#pragma once
#include <algorithm>
#include <cstdint>

#include "velographx/graph_access.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

template <class Graph>
class BasicIncrementalTriangleCount {
 public:
  explicit BasicIncrementalTriangleCount(Graph& graph) : graph_(graph) { recompute(); }
  BasicIncrementalTriangleCount(Graph& graph, std::uint64_t trusted_initial_count)
      : graph_(graph), triangles_(trusted_initial_count) {}

  [[nodiscard]] std::uint64_t value() const noexcept { return triangles_; }

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) return;
    for (const auto& op : batch.updates) {
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
    triangles_ = is_directed(graph_) ? triple : triple / 3;
  }

 protected:
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

// DynamicGraph forward-declares/friends this historical public type. Keep a
// thin specialization wrapper so one logical UpdateBatch still advances the
// graph version exactly once while later operations observe earlier updates.
class IncrementalTriangleCount : public BasicIncrementalTriangleCount<DynamicGraph> {
 public:
  explicit IncrementalTriangleCount(DynamicGraph& graph)
      : BasicIncrementalTriangleCount<DynamicGraph>(graph) {}
  IncrementalTriangleCount(DynamicGraph& graph, std::uint64_t trusted_initial_count)
      : BasicIncrementalTriangleCount<DynamicGraph>(graph, trusted_initial_count) {}

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) return;
    for (const auto& op : batch.updates) {
      const bool exists = graph_.has_edge(op.src, op.dst);
      const auto common = common_neighbors(op.src, op.dst);
      if (op.add && !exists) triangles_ += common;
      if (!op.add && exists) triangles_ -= std::min<std::uint64_t>(triangles_, common);
      graph_.apply_unversioned(op);
    }
    ++graph_.version_;
  }
};

} // namespace velographx
