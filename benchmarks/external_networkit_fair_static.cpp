#include <networkit/graph/BFS.hpp>
#include <networkit/graph/Dijkstra.hpp>
#include <networkit/graph/Graph.hpp>
#include <networkit/graph/GraphBuilder.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Edge = std::pair<NetworKit::node, NetworKit::node>;
using Clock = std::chrono::steady_clock;

std::vector<Edge> read_edges(const std::string &path, std::size_t &vertices) {
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
    edges.emplace_back(static_cast<NetworKit::node>(u), static_cast<NetworKit::node>(v));
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw = true;
  }
  vertices = saw ? static_cast<std::size_t>(max_vertex + 1) : 0;
  return edges;
}

NetworKit::Graph build_graph(const std::vector<Edge> &edges, std::size_t n) {
  // GraphBuilder is NetworKit's bulk-construction path. autoCompleteEdges=true
  // supplies both half-edge structures required by a directed Graph.
  NetworKit::GraphBuilder builder(static_cast<NetworKit::count>(n), false, true, true);
  for (const auto &[u, v] : edges) builder.addHalfEdge(u, v);
  return builder.completeGraph();
}

struct TraversalResult {
  std::uint64_t visited = 0;
  std::uint64_t distance_sum = 0;
};

TraversalResult run_bfs(const NetworKit::Graph &graph, NetworKit::node root) {
  TraversalResult out;
  NetworKit::Traversal::BFSfrom(graph, root, [&](NetworKit::node, NetworKit::count distance) {
    ++out.visited;
    out.distance_sum += static_cast<std::uint64_t>(distance);
  });
  return out;
}

TraversalResult run_dijkstra(const NetworKit::Graph &graph, NetworKit::node root) {
  TraversalResult out;
  NetworKit::Traversal::DijkstraFrom(graph, root, [&](NetworKit::node, NetworKit::edgeweight distance) {
    ++out.visited;
    out.distance_sum += static_cast<std::uint64_t>(distance);
  });
  return out;
}

double median(std::vector<double> values) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const auto mid = values.size() / 2;
  if (values.size() % 2) return values[mid];
  return (values[mid - 1] + values[mid]) / 2.0;
}

void print_samples(const std::vector<double> &values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}
}  // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "usage: " << argv[0] << " edge_list root repetitions\n";
    return 2;
  }
  const std::string path = argv[1];
  const auto root = static_cast<NetworKit::node>(std::stoull(argv[2]));
  const auto repetitions = static_cast<std::size_t>(std::stoull(argv[3]));
  if (repetitions == 0) return 2;

  std::size_t n = 0;
  const auto parse_start = Clock::now();
  const auto edges = read_edges(path, n);
  const auto parse_end = Clock::now();
  if (n == 0 || edges.empty() || root >= n) return 2;

  const auto build_start = Clock::now();
  auto graph = build_graph(edges, n);
  const auto build_end = Clock::now();
  if (graph.numberOfEdges() != edges.size()) {
    throw std::runtime_error("GraphBuilder edge count differs from normalized input; duplicate edges are not valid for this contract");
  }

  // One identical, unmeasured warm-up for each traversal.
  const auto warm_bfs = run_bfs(graph, root);
  const auto warm_dijkstra = run_dijkstra(graph, root);
  if (warm_bfs.visited != warm_dijkstra.visited || warm_bfs.distance_sum != warm_dijkstra.distance_sum) {
    throw std::runtime_error("BFS and unit-weight Dijkstra disagree during warm-up");
  }

  std::vector<double> bfs_samples;
  std::vector<double> dijkstra_samples;
  bfs_samples.reserve(repetitions);
  dijkstra_samples.reserve(repetitions);
  TraversalResult bfs_reference = warm_bfs;

  for (std::size_t rep = 0; rep < repetitions; ++rep) {
    const auto t0 = Clock::now();
    const auto bfs = run_bfs(graph, root);
    const auto t1 = Clock::now();
    const auto dijkstra = run_dijkstra(graph, root);
    const auto t2 = Clock::now();
    if (bfs.visited != bfs_reference.visited || bfs.distance_sum != bfs_reference.distance_sum ||
        dijkstra.visited != bfs_reference.visited || dijkstra.distance_sum != bfs_reference.distance_sum) {
      throw std::runtime_error("traversal result changed across repetitions");
    }
    bfs_samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    dijkstra_samples.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
  }

  const double parse_us = std::chrono::duration<double, std::micro>(parse_end - parse_start).count();
  const double build_us = std::chrono::duration<double, std::micro>(build_end - build_start).count();
  const double bfs_median_us = median(bfs_samples);
  const double dijkstra_median_us = median(dijkstra_samples);

  std::cout << "{\"schema_version\":1,"
            << "\"artifact_type\":\"velographx-networkit-fair-static-contract\","
            << "\"native_cpp\":true,\"directed\":true,\"weighted\":false,"
            << "\"graph_builder\":\"NetworKit::GraphBuilder\","
            << "\"bfs_api\":\"NetworKit::Traversal::BFSfrom(callback)\","
            << "\"dijkstra_api\":\"NetworKit::Traversal::DijkstraFrom(callback)\","
            << "\"path_storage\":false,\"warmup_runs_per_algorithm\":1,"
            << "\"vertices\":" << n << ",\"edges\":" << edges.size() << ','
            << "\"root\":" << root << ",\"repetitions\":" << repetitions << ','
            << "\"parse_us\":" << parse_us << ",\"graph_build_us\":" << build_us << ','
            << "\"bfs_algorithm_median_us\":" << bfs_median_us << ','
            << "\"dijkstra_algorithm_median_us\":" << dijkstra_median_us << ','
            << "\"bfs_build_plus_algorithm_median_us\":" << (build_us + bfs_median_us) << ','
            << "\"dijkstra_build_plus_algorithm_median_us\":" << (build_us + dijkstra_median_us) << ','
            << "\"visited_vertices\":" << bfs_reference.visited << ','
            << "\"distance_sum\":" << bfs_reference.distance_sum << ','
            << "\"bfs_samples_us\":";
  print_samples(bfs_samples);
  std::cout << ",\"dijkstra_samples_us\":";
  print_samples(dijkstra_samples);
  std::cout << ",\"bfs_equals_unit_weight_dijkstra\":true,"
            << "\"guidance_source\":\"personal technical advice from Mikhail Kirilin\","
            << "\"networkit_endorsement\":false,\"research_claim\":false}\n";
  return 0;
}
