#include "velographx/algorithms.hpp"
#include "velographx/csr_graph.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Edge = velographx::CsrGraph::Edge;

std::vector<Edge> read_edges(const std::string& path, std::size_t& vertices) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list: " + path);
  std::vector<Edge> edges;
  std::string line;
  std::uint64_t max_vertex = 0;
  bool saw = false;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::uint64_t u = 0, v = 0;
    if (!(row >> u >> v)) continue;
    edges.emplace_back(static_cast<velographx::VertexId>(u), static_cast<velographx::VertexId>(v));
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw = true;
  }
  vertices = saw ? static_cast<std::size_t>(max_vertex + 1) : 0;
  return edges;
}

struct Result { std::uint64_t visited = 0; std::uint64_t distance_sum = 0; };

Result run_bfs(const velographx::CsrGraph& graph, velographx::VertexId root) {
  const auto distances = velographx::bfs_distances(graph, root);
  Result out;
  for (const auto d : distances) {
    if (d != UINT32_MAX) { ++out.visited; out.distance_sum += d; }
  }
  return out;
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  const auto mid = values.size() / 2;
  return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2.0;
}

void print_samples(const std::vector<double>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << values[i]; }
  std::cout << ']';
}
}

int main(int argc, char** argv) {
  if (argc != 4) { std::cerr << "usage: " << argv[0] << " edge_list root repetitions\n"; return 2; }
  const std::string path = argv[1];
  const auto root = static_cast<velographx::VertexId>(std::stoull(argv[2]));
  const auto repetitions = static_cast<std::size_t>(std::stoull(argv[3]));
  if (repetitions == 0) return 2;

  std::size_t n = 0;
  const auto parse_start = Clock::now();
  auto edges = read_edges(path, n);
  const auto parse_end = Clock::now();
  if (n == 0 || edges.empty() || root >= n) return 2;

  const auto build_start = Clock::now();
  velographx::CsrGraph graph(std::move(edges), true);
  const auto build_end = Clock::now();

  const auto warm = run_bfs(graph, root);
  std::vector<double> samples;
  samples.reserve(repetitions);
  for (std::size_t rep = 0; rep < repetitions; ++rep) {
    const auto t0 = Clock::now();
    const auto result = run_bfs(graph, root);
    const auto t1 = Clock::now();
    if (result.visited != warm.visited || result.distance_sum != warm.distance_sum)
      throw std::runtime_error("BFS result changed across repetitions");
    samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
  }

  const double parse_us = std::chrono::duration<double, std::micro>(parse_end - parse_start).count();
  const double build_us = std::chrono::duration<double, std::micro>(build_end - build_start).count();
  const double bfs_us = median(samples);
  std::cout << "{\"schema_version\":1,\"artifact_type\":\"velographx-fair-static-bfs\","
            << "\"native_cpp\":true,\"directed\":true,\"weighted\":false,"
            << "\"warmup_runs\":1,\"vertices\":" << n << ",\"edges\":" << graph.edge_entry_count() << ','
            << "\"root\":" << root << ",\"repetitions\":" << repetitions << ','
            << "\"parse_us\":" << parse_us << ",\"graph_build_us\":" << build_us << ','
            << "\"bfs_algorithm_median_us\":" << bfs_us << ','
            << "\"bfs_build_plus_algorithm_median_us\":" << (build_us + bfs_us) << ','
            << "\"visited_vertices\":" << warm.visited << ",\"distance_sum\":" << warm.distance_sum << ','
            << "\"bfs_samples_us\":"; print_samples(samples);
  std::cout << ",\"research_claim\":false}\n";
}
