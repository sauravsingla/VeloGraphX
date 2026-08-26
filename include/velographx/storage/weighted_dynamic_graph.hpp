#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

using EdgeWeight = std::uint64_t;

struct WeightedEdgeUpdate {
  VertexId src{};
  VertexId dst{};
  EdgeWeight weight{1};
  bool add{true};
  std::uint64_t timestamp{0};
};

struct WeightedUpdateBatch {
  std::vector<WeightedEdgeUpdate> updates;

  void add(VertexId u, VertexId v, EdgeWeight w, std::uint64_t ts = 0) {
    updates.push_back({u, v, w, true, ts});
  }

  void remove(VertexId u, VertexId v, std::uint64_t ts = 0) {
    updates.push_back({u, v, 0, false, ts});
  }

  void update(VertexId u, VertexId v, EdgeWeight w, std::uint64_t ts = 0) {
    updates.push_back({u, v, w, true, ts});
  }

  [[nodiscard]] bool empty() const noexcept { return updates.empty(); }
};

class WeightedDynamicGraph {
 public:
  explicit WeightedDynamicGraph(std::size_t vertices = 0, bool directed = false)
      : directed_(directed), adjacency_(vertices) {}

  [[nodiscard]] std::size_t vertex_count() const noexcept { return adjacency_.size(); }
  [[nodiscard]] std::uint64_t version() const noexcept { return version_; }
  [[nodiscard]] bool directed() const noexcept { return directed_; }

  void ensure_vertex(VertexId v) {
    const auto n = static_cast<std::size_t>(v) + 1;
    if (n > adjacency_.size()) adjacency_.resize(n);
  }

  void apply(const WeightedUpdateBatch& batch) {
    for (const auto& op : batch.updates) {
      ensure_vertex(std::max(op.src, op.dst));
      apply_one(op);
      if (!directed_ && op.src != op.dst) {
        WeightedEdgeUpdate reverse{op.dst, op.src, op.weight, op.add, op.timestamp};
        apply_one(reverse);
      }
    }
    if (!batch.empty()) ++version_;
  }

  [[nodiscard]] std::optional<EdgeWeight> weight(VertexId u, VertexId v) const {
    if (u >= adjacency_.size()) return std::nullopt;
    const auto it = adjacency_[u].find(v);
    if (it == adjacency_[u].end()) return std::nullopt;
    return it->second;
  }

  [[nodiscard]] std::vector<std::pair<VertexId, EdgeWeight>> neighbors(VertexId u) const {
    if (u >= adjacency_.size()) return {};
    std::vector<std::pair<VertexId, EdgeWeight>> out;
    out.reserve(adjacency_[u].size());
    for (const auto& [v, w] : adjacency_[u]) out.push_back({v, w});
    return out;
  }

 private:
  void apply_one(const WeightedEdgeUpdate& op) {
    if (op.add) {
      adjacency_[op.src][op.dst] = op.weight;
    } else {
      adjacency_[op.src].erase(op.dst);
    }
  }

  bool directed_{false};
  std::vector<std::map<VertexId, EdgeWeight>> adjacency_;
  std::uint64_t version_{0};
};

}  // namespace velographx
