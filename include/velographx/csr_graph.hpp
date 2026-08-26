#pragma once
#include "velographx/types.hpp"
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
  [[nodiscard]] std::size_t degree(VertexId v) const;

private:
  bool directed_{false};
  std::size_t vertex_count_{0};
  std::vector<EdgeOffset> offsets_{0};
  std::vector<VertexId> neighbors_;
};

}
