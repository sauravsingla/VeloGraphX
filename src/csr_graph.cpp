#include "velographx/csr_graph.hpp"
#include <algorithm>
#include <stdexcept>

namespace velographx {

CsrGraph::CsrGraph(std::vector<Edge> edges, bool directed) : directed_(directed) {
  std::vector<Edge> expanded;
  expanded.reserve(directed ? edges.size() : edges.size() * 2);
  VertexId max_vertex = 0;
  bool has_vertex = false;
  for (const auto& [u, v] : edges) {
    expanded.emplace_back(u, v);
    if (!directed && u != v) expanded.emplace_back(v, u);
    max_vertex = std::max({max_vertex, u, v});
    has_vertex = true;
  }
  vertex_count_ = has_vertex ? static_cast<std::size_t>(max_vertex) + 1 : 0;
  std::sort(expanded.begin(), expanded.end());
  expanded.erase(std::unique(expanded.begin(), expanded.end()), expanded.end());

  offsets_.assign(vertex_count_ + 1, 0);
  for (const auto& [u, v] : expanded) {
    (void)v;
    ++offsets_[static_cast<std::size_t>(u) + 1];
  }
  for (std::size_t i = 1; i < offsets_.size(); ++i) offsets_[i] += offsets_[i - 1];

  neighbors_.resize(expanded.size());
  auto cursor = offsets_;
  for (const auto& [u, v] : expanded) neighbors_[cursor[u]++] = v;
}

std::span<const VertexId> CsrGraph::neighbors(VertexId v) const {
  if (v >= vertex_count_) throw std::out_of_range("vertex id outside graph");
  const auto begin = offsets_[v];
  const auto end = offsets_[static_cast<std::size_t>(v) + 1];
  return {neighbors_.data() + begin, static_cast<std::size_t>(end - begin)};
}

std::size_t CsrGraph::degree(VertexId v) const { return neighbors(v).size(); }

}
