#pragma once
#include <algorithm>
#include <cstdint>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/kernels/intersection.hpp"

namespace velographx {

class IncrementalTriangleCount {
 public:
  explicit IncrementalTriangleCount(DynamicGraph& graph) : graph_(graph) { recompute(); }
  [[nodiscard]] std::uint64_t value() const noexcept { return triangles_; }

  void apply(const UpdateBatch& batch) {
    if (batch.empty()) return;

    for (const auto& op : batch.updates) {
      const bool exists = graph_.has_edge(op.src, op.dst);
      const auto a = graph_.neighbors(op.src);
      const auto b = graph_.neighbors(op.dst);
      const auto common = kernels::adaptive_intersection(a, b);

      if (op.add && !exists) triangles_ += common;
      if (!op.add && exists) triangles_ -= std::min<std::uint64_t>(triangles_, common);

      // Advance the graph after each operation so later operations in the same
      // batch observe earlier changes. The batch version is still bumped once.
      graph_.apply_unversioned(op);
    }
    ++graph_.version_;
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
