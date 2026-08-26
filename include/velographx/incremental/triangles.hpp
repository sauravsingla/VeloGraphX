#pragma once
#include <cstdint>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/kernels/intersection.hpp"

namespace velographx {

class IncrementalTriangleCount {
 public:
  explicit IncrementalTriangleCount(DynamicGraph& graph) : graph_(graph) { recompute(); }
  [[nodiscard]] std::uint64_t value() const noexcept { return triangles_; }

  void apply(const UpdateBatch& batch) {
    for (const auto& op : batch.updates) {
      auto a = graph_.neighbors(op.src);
      auto b = graph_.neighbors(op.dst);
      const auto common = kernels::adaptive_intersection(a, b);
      if (op.add && !graph_.has_edge(op.src, op.dst)) triangles_ += common;
      if (!op.add && graph_.has_edge(op.src, op.dst)) triangles_ -= std::min<std::uint64_t>(triangles_, common);
    }
    graph_.apply(batch);
  }

  void recompute() {
    std::uint64_t triple = 0;
    for (VertexId u = 0; u < graph_.vertex_count(); ++u) {
      auto nu = graph_.neighbors(u);
      for (auto v : nu) if (u < v) {
        auto nv = graph_.neighbors(v);
        triple += kernels::adaptive_intersection(nu, nv);
      }
    }
    triangles_ = graph_.directed() ? triple : triple / 3;
  }

 private:
  DynamicGraph& graph_;
  std::uint64_t triangles_{0};
};
} // namespace velographx
