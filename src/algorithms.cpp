#include "velographx/algorithms.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>

namespace velographx {

std::vector<std::uint32_t> bfs_distances(const CsrGraph& graph, VertexId source) {
  if (source >= graph.vertex_count()) throw std::out_of_range("source outside graph");
  std::vector<std::uint32_t> distance(graph.vertex_count(), kUnreachable);
  std::queue<VertexId> q;
  distance[source] = 0;
  q.push(source);
  while (!q.empty()) {
    const auto u = q.front(); q.pop();
    for (const auto v : graph.neighbors(u)) {
      if (distance[v] == kUnreachable) {
        distance[v] = distance[u] + 1;
        q.push(v);
      }
    }
  }
  return distance;
}

std::vector<VertexId> connected_components(const CsrGraph& graph) {
  std::vector<VertexId> component(graph.vertex_count(), std::numeric_limits<VertexId>::max());
  VertexId cid = 0;
  std::queue<VertexId> q;
  for (VertexId start = 0; start < graph.vertex_count(); ++start) {
    if (component[start] != std::numeric_limits<VertexId>::max()) continue;
    component[start] = cid;
    q.push(start);
    while (!q.empty()) {
      const auto u = q.front(); q.pop();
      for (const auto v : graph.neighbors(u)) {
        if (component[v] == std::numeric_limits<VertexId>::max()) {
          component[v] = cid;
          q.push(v);
        }
      }
    }
    ++cid;
  }
  return component;
}

std::vector<double> pagerank(const CsrGraph& graph, double damping, std::size_t max_iterations, double tolerance) {
  const auto n = graph.vertex_count();
  if (n == 0) return {};
  std::vector<double> rank(n, 1.0 / static_cast<double>(n));
  std::vector<double> next(n);
  for (std::size_t iter = 0; iter < max_iterations; ++iter) {
    double dangling = 0.0;
    for (VertexId u = 0; u < n; ++u) if (graph.degree(u) == 0) dangling += rank[u];
    const double base = (1.0 - damping) / static_cast<double>(n) + damping * dangling / static_cast<double>(n);
    std::fill(next.begin(), next.end(), base);
    for (VertexId u = 0; u < n; ++u) {
      const auto nbrs = graph.neighbors(u);
      if (nbrs.empty()) continue;
      const double contribution = damping * rank[u] / static_cast<double>(nbrs.size());
      for (const auto v : nbrs) next[v] += contribution;
    }
    double error = 0.0;
    for (std::size_t i = 0; i < n; ++i) error += std::abs(next[i] - rank[i]);
    rank.swap(next);
    if (error < tolerance) break;
  }
  return rank;
}

std::uint64_t common_neighbor_count(const CsrGraph& graph, VertexId u, VertexId v) {
  const auto a = graph.neighbors(u);
  const auto b = graph.neighbors(v);
  std::size_t i = 0, j = 0;
  std::uint64_t count = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) { ++count; ++i; ++j; }
    else if (a[i] < b[j]) ++i;
    else ++j;
  }
  return count;
}

double jaccard_similarity(const CsrGraph& graph, VertexId u, VertexId v) {
  const auto intersection = common_neighbor_count(graph, u, v);
  const auto union_size = graph.degree(u) + graph.degree(v) - intersection;
  return union_size == 0 ? 0.0 : static_cast<double>(intersection) / static_cast<double>(union_size);
}

std::uint64_t triangle_count(const CsrGraph& graph) {
  if (graph.directed()) throw std::invalid_argument("triangle_count currently requires an undirected graph");
  std::uint64_t triangles = 0;
  for (VertexId u = 0; u < graph.vertex_count(); ++u) {
    const auto nu = graph.neighbors(u);
    for (const auto v : nu) {
      if (v <= u) continue;
      const auto nv = graph.neighbors(v);
      std::size_t i = 0, j = 0;
      while (i < nu.size() && j < nv.size()) {
        const auto a = nu[i], b = nv[j];
        if (a <= v) { ++i; continue; }
        if (b <= v) { ++j; continue; }
        if (a == b) { ++triangles; ++i; ++j; }
        else if (a < b) ++i;
        else ++j;
      }
    }
  }
  return triangles;
}

}
