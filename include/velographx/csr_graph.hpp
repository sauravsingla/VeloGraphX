#pragma once
#include "velographx/types.hpp"
#include <algorithm>
#include <span>
#include <utility>
#include <vector>

namespace velographx {

class CsrGraph {
public:
  using Edge = std::pair<VertexId, VertexId>;

  CsrGraph() = default;
  CsrGraph(std::vector<Edge> edges, bool directed = false);

  [[nodiscard]] std::size_t vertex_count() const noexcept { return vertex_count_; }
  [[nodiscard]] std::size_t edge_entry_count() const noexcept { return neighbors_.size(); }
  [[nodiscard]] bool directed() const noexcept { return directed_; }
  [[nodiscard]] std::span<const VertexId> neighbors(VertexId v) const;
  [[nodiscard]] std::span<const VertexId> in_neighbors(VertexId v) const;
  [[nodiscard]] std::size_t degree(VertexId v) const;
  [[nodiscard]] bool has_edge(VertexId u, VertexId v) const;

  template <class Fn>
  void for_each_neighbor(VertexId v, Fn&& fn) const {
    for (const auto dst : neighbors(v)) fn(dst);
  }

  template <class Fn>
  void for_each_in_neighbor(VertexId v, Fn&& fn) const {
    for (const auto src : in_neighbors(v)) fn(src);
  }

private:
  bool directed_{false};
  std::size_t vertex_count_{0};
  std::vector<EdgeOffset> offsets_{0};
  std::vector<VertexId> neighbors_;
  std::vector<EdgeOffset> in_offsets_{0};
  std::vector<VertexId> in_neighbors_;
};

}
