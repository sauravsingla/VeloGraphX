#pragma once

#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include "velographx/incremental/dijkstra.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

namespace velographx {

class IncrementalWeightedSSSP {
 public:
  static constexpr std::uint64_t kInf = incremental_detail::kDijkstraInf;

  IncrementalWeightedSSSP(WeightedDynamicGraph& graph, VertexId source)
      : graph_(graph), source_(source) {
    recompute();
  }

  [[nodiscard]] const std::vector<std::uint64_t>& distances() const noexcept { return dist_; }

  void apply(const WeightedUpdateBatch& batch) {
    bool requires_recompute = false;
    for (const auto& op : batch.updates) {
      if (!op.add) {
        requires_recompute = true;
        break;
      }
      const auto old_weight = graph_.weight(op.src, op.dst);
      if (old_weight && op.weight > *old_weight) {
        requires_recompute = true;
        break;
      }
    }

    graph_.apply(batch);
    if (requires_recompute) recompute();
    else relax_from_updates(batch);
  }

  void recompute() {
    incremental_detail::recompute_dijkstra(
        graph_.vertex_count(), source_, dist_,
        [&](VertexId u, auto&& relax) {
          graph_.for_each_neighbor(u, [&](VertexId v, EdgeWeight w) { relax(v, w); });
        });
  }

 private:
  void relax_from_updates(const WeightedUpdateBatch& batch) {
    if (dist_.size() < graph_.vertex_count()) dist_.resize(graph_.vertex_count(), kInf);

    using Item = std::pair<std::uint64_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;

    for (const auto& op : batch.updates) {
      if (!op.add || op.src >= dist_.size() || op.dst >= dist_.size()) continue;
      if (dist_[op.src] != kInf && op.weight <= kInf - dist_[op.src]) {
        const auto candidate = dist_[op.src] + op.weight;
        if (candidate < dist_[op.dst]) {
          dist_[op.dst] = candidate;
          queue.push({candidate, op.dst});
        }
      }
      if (!graph_.directed() && dist_[op.dst] != kInf && op.weight <= kInf - dist_[op.dst]) {
        const auto reverse_candidate = dist_[op.dst] + op.weight;
        if (reverse_candidate < dist_[op.src]) {
          dist_[op.src] = reverse_candidate;
          queue.push({reverse_candidate, op.src});
        }
      }
    }

    incremental_detail::propagate_dijkstra(
        dist_, queue,
        [&](VertexId u, auto&& relax) {
          graph_.for_each_neighbor(u, [&](VertexId v, EdgeWeight w) { relax(v, w); });
        });
  }

  WeightedDynamicGraph& graph_;
  VertexId source_;
  std::vector<std::uint64_t> dist_;
};

}  // namespace velographx
