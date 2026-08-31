#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Clock = std::chrono::steady_clock;
using velographx::BasicIncrementalBFS;
using velographx::CsrGraph;
using velographx::DynamicGraph;
using velographx::VertexId;

std::vector<std::pair<VertexId, VertexId>> make_edges(std::size_t vertices, std::size_t degree) {
  if (vertices < 2 || degree == 0) throw std::invalid_argument("vertices and degree must be positive");
  std::vector<std::pair<VertexId, VertexId>> edges;
  edges.reserve(vertices * degree);
  for (std::size_t u = 0; u < vertices; ++u) {
    for (std::size_t d = 1; d <= degree; ++d) {
      const auto v = (u + d) % vertices;
      if (u != v) edges.emplace_back(static_cast<VertexId>(u), static_cast<VertexId>(v));
    }
  }
  return edges;
}

template <class Graph>
double run(Graph& graph, VertexId source, std::vector<std::uint32_t>& out) {
  const auto begin = Clock::now();
  BasicIncrementalBFS<Graph> bfs(graph, source);
  const auto end = Clock::now();
  out = bfs.distances();
  return std::chrono::duration<double, std::milli>(end - begin).count();
}
}  // namespace

int main(int argc, char** argv) {
  const auto vertices = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 100000;
  const auto degree = argc > 2 ? static_cast<std::size_t>(std::stoull(argv[2])) : 8;
  const auto source = argc > 3 ? static_cast<VertexId>(std::stoul(argv[3])) : 0;

  auto edges = make_edges(vertices, degree);
  DynamicGraph dynamic(vertices, false);
  dynamic.bulk_load_edges(edges);
  CsrGraph csr(edges, false);

  std::vector<std::uint32_t> dynamic_distances;
  std::vector<std::uint32_t> csr_distances;
  const auto dynamic_ms = run(dynamic, source, dynamic_distances);
  const auto csr_ms = run(csr, source, csr_distances);

  if (dynamic_distances != csr_distances) {
    std::cerr << "backend BFS correctness mismatch\n";
    return 1;
  }

  std::cout << "{\"algorithm\":\"BasicIncrementalBFS::recompute\","
            << "\"vertices\":" << vertices << ','
            << "\"degree\":" << degree << ','
            << "\"source\":" << source << ','
            << "\"dynamic_ms\":" << dynamic_ms << ','
            << "\"csr_ms\":" << csr_ms << ','
            << "\"correctness_match\":true}\n";
  return 0;
}
