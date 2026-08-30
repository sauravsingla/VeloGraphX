#include <networkit/distance/BFS.hpp>
#include <networkit/distance/DynBFS.hpp>
#include <networkit/dynamics/GraphEvent.hpp>
#include <networkit/graph/Graph.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Edge = std::pair<NetworKit::node, NetworKit::node>;

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
    edges.emplace_back(static_cast<NetworKit::node>(u), static_cast<NetworKit::node>(v));
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw = true;
  }
  vertices = saw ? static_cast<std::size_t>(max_vertex + 1) : 0;
  return edges;
}

std::vector<std::int64_t> distances(const NetworKit::SSSP& bfs, std::size_t n) {
  std::vector<std::int64_t> out(n, -1);
  for (std::size_t u = 0; u < n; ++u) {
    const auto d = bfs.distance(static_cast<NetworKit::node>(u));
    if (d < std::numeric_limits<NetworKit::edgeweight>::max()) out[u] = static_cast<std::int64_t>(d);
  }
  return out;
}

std::vector<std::uint64_t> layer_counts(const std::vector<std::int64_t>& dist) {
  std::int64_t max_depth = 0;
  for (auto d : dist) if (d >= 0) max_depth = std::max(max_depth, d);
  std::vector<std::uint64_t> counts(static_cast<std::size_t>(max_depth) + 1, 0);
  for (auto d : dist) if (d >= 0) ++counts[static_cast<std::size_t>(d)];
  return counts;
}

void print_u64_array(const std::vector<std::uint64_t>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}
}

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: " << argv[0] << " edge_list root imported_rate batch_size\n";
    return 2;
  }
  const std::string path = argv[1];
  const auto root = static_cast<NetworKit::node>(std::stoull(argv[2]));
  const double imported_rate = std::stod(argv[3]);
  const std::size_t batch_size = std::stoull(argv[4]);
  if (!(imported_rate > 0.0 && imported_rate < 1.0) || batch_size == 0) return 2;

  std::size_t n = 0;
  const auto edges = read_edges(path, n);
  const auto imported = static_cast<std::size_t>(edges.size() * imported_rate);
  if (n == 0 || imported == 0 || imported >= edges.size() || root >= n) return 2;

  NetworKit::Graph graph(static_cast<NetworKit::count>(n), false, true);
  for (std::size_t i = 0; i < imported; ++i) graph.addEdge(edges[i].first, edges[i].second, 1.0, true);
  if (graph.numberOfEdges() != imported) {
    throw std::runtime_error("initial edge count mismatch; duplicate edge violates simple-graph contract");
  }

  NetworKit::DynBFS dyn(graph, root, false);
  dyn.run();

  std::size_t batches = 0;
  std::size_t operations = 0;
  double total_us = 0.0;
  double mutation_us = 0.0;
  double maintenance_us = 0.0;

  for (std::size_t begin = imported; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    std::vector<NetworKit::GraphEvent> events;
    events.reserve((end - begin) * 2);

    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = begin; i < end; ++i) {
      graph.addEdge(edges[i].first, edges[i].second, 1.0, true);
      events.emplace_back(NetworKit::GraphEvent::EDGE_ADDITION, edges[i].first, edges[i].second, 1.0);
    }
    for (std::size_t i = begin; i < end; ++i) {
      const auto j = i - imported;
      graph.removeEdge(edges[j].first, edges[j].second);
      events.emplace_back(NetworKit::GraphEvent::EDGE_REMOVAL, edges[j].first, edges[j].second, 1.0);
    }
    const auto t1 = std::chrono::steady_clock::now();
    dyn.updateBatch(events);
    const auto t2 = std::chrono::steady_clock::now();

    mutation_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    maintenance_us += std::chrono::duration<double, std::micro>(t2 - t1).count();
    total_us += std::chrono::duration<double, std::micro>(t2 - t0).count();
    operations += events.size();
    ++batches;

    NetworKit::BFS full(graph, root, false, false);
    full.run();
    if (distances(dyn, n) != distances(full, n)) {
      throw std::runtime_error("NetworKit DynBFS mismatch against fresh full BFS after batch " + std::to_string(batches));
    }
  }

  if (graph.numberOfEdges() != imported) throw std::runtime_error("final edge-count mismatch");
  const auto final_dist = distances(dyn, n);
  const auto layers = layer_counts(final_dist);
  std::uint64_t visited = 0;
  for (auto x : layers) visited += x;

  std::cout << "{\"schema_version\":1,"
            << "\"artifact_type\":\"velographx-external-networkit-native-dynbfs\","
            << "\"native_cpp\":true,\"directed\":true,\"weighted\":false,"
            << "\"vertices\":" << n << ",\"source_edges\":" << edges.size() << ','
            << "\"initial_edges\":" << imported << ",\"final_edges\":" << graph.numberOfEdges() << ','
            << "\"root\":" << root << ",\"imported_rate\":" << imported_rate << ','
            << "\"batch_size\":" << batch_size << ",\"batches\":" << batches << ','
            << "\"update_operations\":" << operations << ','
            << "\"wall_total_us\":" << total_us << ','
            << "\"wall_mean_us\":" << (batches ? total_us / batches : 0.0) << ','
            << "\"mutation_total_us\":" << mutation_us << ','
            << "\"maintenance_total_us\":" << maintenance_us << ','
            << "\"all_batches_correct_against_fresh_full_bfs\":true,"
            << "\"correctness_batches\":" << batches << ','
            << "\"visited_vertices\":" << visited << ",\"layer_counts\":";
  print_u64_array(layers);
  std::cout << ",\"research_claim\":false}\n";
  return 0;
}
