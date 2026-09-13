#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

#include "velographx/graph_access.hpp"
#include "velographx/incremental/dijkstra.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

namespace velographx {

template <class Graph>
class BasicIncrementalWeightedSSSP {
 public:
  static constexpr std::uint64_t kInf = incremental_detail::kDijkstraInf;

  BasicIncrementalWeightedSSSP(Graph& graph, VertexId source)
      : graph_(graph), source_(source) {
    recompute();
  }

  [[nodiscard]] const std::vector<std::uint64_t>& distances() const noexcept { return dist_; }

  void apply(const WeightedUpdateBatch& batch) {
    if (batch.empty()) return;

    // Validate before calling an arbitrary graph backend so a rejected weight
    // cannot leave a partially applied foreign graph.
    for (const auto& op : batch.updates) {
      if (op.src != op.dst && op.add && op.weight >= kInf) {
        throw std::invalid_argument(
            "edge weight exceeds the representable finite-distance domain");
      }
    }

    const auto canonical = canonicalize(batch);
    bool requires_recompute = false;
    for (const auto& op : canonical.updates) {
      const auto old_weight = edge_weight(graph_, op.src, op.dst);
      if (!op.add) {
        if (old_weight.has_value()) {
          requires_recompute = true;
          break;
        }
        continue;
      }
      if (old_weight && op.weight > *old_weight) {
        requires_recompute = true;
        break;
      }
    }

    apply_updates(graph_, batch);
    if (requires_recompute) recompute();
    else relax_from_updates(canonical);
  }

  void recompute() {
    incremental_detail::recompute_dijkstra(
        vertex_count(graph_), source_, dist_,
        [&](VertexId u, auto&& relax) {
          for_each_weighted_neighbor(graph_, u, [&](VertexId v, auto w) { relax(v, w); });
        });
  }

 private:
  static std::uint64_t edge_key(VertexId u, VertexId v, bool directed) noexcept {
    if (!directed && v < u) std::swap(u, v);
    return (static_cast<std::uint64_t>(u) << 32U) | static_cast<std::uint64_t>(v);
  }

  WeightedUpdateBatch canonicalize(const WeightedUpdateBatch& batch) const {
    WeightedUpdateBatch out;
    out.updates.reserve(batch.updates.size());
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(batch.updates.size() * 2 + 1);

    for (auto it = batch.updates.rbegin(); it != batch.updates.rend(); ++it) {
      if (it->src == it->dst) continue;
      auto op = *it;
      if (!is_directed(graph_) && op.dst < op.src) std::swap(op.src, op.dst);
      if (seen.insert(edge_key(op.src, op.dst, is_directed(graph_))).second) {
        out.updates.push_back(op);
      }
    }
    std::reverse(out.updates.begin(), out.updates.end());
    return out;
  }

  void relax_from_updates(const WeightedUpdateBatch& batch) {
    if (dist_.size() < vertex_count(graph_)) dist_.resize(vertex_count(graph_), kInf);

    using Item = std::pair<std::uint64_t, VertexId>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;

    for (const auto& op : batch.updates) {
      if (!op.add || op.src >= dist_.size() || op.dst >= dist_.size()) continue;
      const auto final_weight = edge_weight(graph_, op.src, op.dst);
      if (!final_weight) continue;
      const auto weight = *final_weight;

      if (dist_[op.src] != kInf && weight <= kInf - dist_[op.src]) {
        const auto candidate = dist_[op.src] + weight;
        if (candidate < dist_[op.dst]) {
          dist_[op.dst] = candidate;
          queue.push({candidate, op.dst});
        }
      }
      if (!is_directed(graph_) && dist_[op.dst] != kInf && weight <= kInf - dist_[op.dst]) {
        const auto reverse_candidate = dist_[op.dst] + weight;
        if (reverse_candidate < dist_[op.src]) {
          dist_[op.src] = reverse_candidate;
          queue.push({reverse_candidate, op.src});
        }
      }
    }

    incremental_detail::propagate_dijkstra(
        dist_, queue,
        [&](VertexId u, auto&& relax) {
          for_each_weighted_neighbor(graph_, u, [&](VertexId v, auto w) { relax(v, w); });
        });
  }

  Graph& graph_;
  VertexId source_;
  std::vector<std::uint64_t> dist_;
};

using IncrementalWeightedSSSP = BasicIncrementalWeightedSSSP<WeightedDynamicGraph>;

}  // namespace velographx
