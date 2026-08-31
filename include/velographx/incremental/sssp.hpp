#pragma once
#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include "velographx/graph_access.hpp"
#include "velographx/incremental/dijkstra.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

template <class Graph>
class BasicIncrementalSSSP {
 public:
  BasicIncrementalSSSP(Graph& g, VertexId source) : g_(g), source_(source) { recompute(); }

  [[nodiscard]] const std::vector<std::uint64_t>& distances() const noexcept { return dist_; }

  void apply(const UpdateBatch& batch) {
    bool deletion = false;
    for (const auto& e : batch.updates) deletion |= !e.add;
    apply_updates(g_, batch);
    if (deletion) recompute();
    else relax_from_updates(batch);
  }

  void recompute() {
    incremental_detail::recompute_dijkstra(
        vertex_count(g_), source_, dist_,
        [&](VertexId u, auto&& relax) {
          for_each_neighbor(g_, u, [&](VertexId v) { relax(v, 1); });
        });
  }

 private:
  void relax_from_updates(const UpdateBatch& batch) {
    if (dist_.size() < vertex_count(g_)) {
      dist_.resize(vertex_count(g_), incremental_detail::kDijkstraInf);
    }
    using Item = std::pair<std::uint64_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
    for (const auto& e : batch.updates) {
      if (!e.add || e.src >= dist_.size() || e.dst >= dist_.size()) continue;
      if (dist_[e.src] != incremental_detail::kDijkstraInf &&
          dist_[e.src] + 1 < dist_[e.dst]) {
        dist_[e.dst] = dist_[e.src] + 1;
        queue.push({dist_[e.dst], e.dst});
      }
      if (!is_directed(g_) && dist_[e.dst] != incremental_detail::kDijkstraInf &&
          dist_[e.dst] + 1 < dist_[e.src]) {
        dist_[e.src] = dist_[e.dst] + 1;
        queue.push({dist_[e.src], e.src});
      }
    }
    incremental_detail::propagate_dijkstra(
        dist_, queue,
        [&](VertexId u, auto&& relax) {
          for_each_neighbor(g_, u, [&](VertexId v) { relax(v, 1); });
        });
  }

  Graph& g_;
  VertexId source_;
  std::vector<std::uint64_t> dist_;
};

using IncrementalSSSP = BasicIncrementalSSSP<DynamicGraph>;

}  // namespace velographx
