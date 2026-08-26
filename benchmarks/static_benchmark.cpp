#include "velographx/algorithms.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

using clock_type = std::chrono::steady_clock;

template <class F> double millis(F&& fn) {
  const auto start = clock_type::now();
  fn();
  return std::chrono::duration<double, std::milli>(clock_type::now() - start).count();
}

int main(int argc, char** argv) {
  const std::size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 10000;
  const std::size_t m = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 50000;
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<std::uint32_t> pick(0, static_cast<std::uint32_t>(n - 1));
  std::vector<velographx::CsrGraph::Edge> edges;
  edges.reserve(m);
  for (std::size_t i = 0; i < m; ++i) edges.emplace_back(pick(rng), pick(rng));

  velographx::CsrGraph graph;
  const double build_ms = millis([&] { graph = velographx::CsrGraph(edges, false); });
  std::vector<std::uint32_t> bfs;
  const double bfs_ms = millis([&] { bfs = velographx::bfs_distances(graph, 0); });
  std::vector<velographx::VertexId> cc;
  const double cc_ms = millis([&] { cc = velographx::connected_components(graph); });
  std::vector<double> pr;
  const double pr_ms = millis([&] { pr = velographx::pagerank(graph, 0.85, 20, 1e-8); });

  std::cout << "{\n"
            << "  \"vertices\": " << graph.vertex_count() << ",\n"
            << "  \"edge_entries\": " << graph.edge_entry_count() << ",\n"
            << "  \"build_ms\": " << build_ms << ",\n"
            << "  \"bfs_ms\": " << bfs_ms << ",\n"
            << "  \"connected_components_ms\": " << cc_ms << ",\n"
            << "  \"pagerank_ms\": " << pr_ms << "\n"
            << "}\n";
}
