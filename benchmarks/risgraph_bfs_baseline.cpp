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

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Edge = std::pair<std::uint64_t, std::uint64_t>;

std::vector<Edge> read_binary_edges(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open graph: " + path);
  std::vector<Edge> edges;
  while (true) {
    std::uint64_t u = 0, v = 0;
    in.read(reinterpret_cast<char*>(&u), sizeof(u));
    if (!in) break;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!in) throw std::runtime_error("truncated binary edge record");
    edges.emplace_back(u, v);
  }
  if (edges.empty()) throw std::runtime_error("graph is empty");
  return edges;
}

std::string layer_histogram(const std::vector<std::uint32_t>& dist) {
  std::uint32_t max_distance = 0;
  for (auto d : dist) {
    if (d != velographx::IncrementalBFS::unreachable) max_distance = std::max(max_distance, d);
  }
  std::vector<std::uint64_t> layers(static_cast<std::size_t>(max_distance) + 1, 0);
  for (auto d : dist) {
    if (d != velographx::IncrementalBFS::unreachable) ++layers[d];
  }
  std::ostringstream out;
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (i) out << ' ';
    out << layers[i];
  }
  return out.str();
}

std::uint64_t distance_digest(const std::vector<std::uint32_t>& dist) {
  std::uint64_t h = 1469598103934665603ULL;
  for (std::size_t i = 0; i < dist.size(); ++i) {
    const std::uint64_t x = (static_cast<std::uint64_t>(i) << 32) ^ dist[i];
    h ^= x;
    h *= 1099511628211ULL;
  }
  return h;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: " << argv[0] << " graph root imported_rate batch_size\n";
    return 2;
  }

  const auto edges = read_binary_edges(argv[1]);
  const std::uint64_t root64 = std::stoull(argv[2]);
  const double imported_rate = std::stod(argv[3]);
  const std::size_t batch_size = static_cast<std::size_t>(std::stoull(argv[4]));
  if (!(imported_rate > 0.0 && imported_rate < 1.0) || batch_size == 0) {
    throw std::runtime_error("invalid imported_rate or batch_size");
  }

  std::uint64_t max_vertex = root64;
  for (const auto& [u, v] : edges) max_vertex = std::max({max_vertex, u, v});
  if (max_vertex >= std::numeric_limits<velographx::VertexId>::max()) {
    throw std::runtime_error("vertex id exceeds VeloGraphX VertexId range");
  }
  const auto vertices = static_cast<std::size_t>(max_vertex + 1);
  const auto root = static_cast<velographx::VertexId>(root64);
  const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
  if (imported_edges == 0 || imported_edges >= edges.size()) {
    throw std::runtime_error("imported prefix must leave a non-empty update stream");
  }

  velographx::DynamicGraph graph(vertices, true);
  for (std::size_t i = 0; i < imported_edges; ++i) {
    graph.add_edge(static_cast<velographx::VertexId>(edges[i].first),
                   static_cast<velographx::VertexId>(edges[i].second));
  }
  velographx::IncrementalBFS bfs(graph, root);
  const auto initial_layers = layer_histogram(bfs.distances());

  std::size_t batches = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t local_begin = imported_edges; local_begin < edges.size(); local_begin += batch_size) {
    const auto local_end = std::min(local_begin + batch_size, edges.size());

    velographx::UpdateBatch additions;
    additions.updates.reserve(local_end - local_begin);
    for (std::size_t i = local_begin; i < local_end; ++i) {
      additions.add(static_cast<velographx::VertexId>(edges[i].first),
                    static_cast<velographx::VertexId>(edges[i].second));
    }
    bfs.apply(additions);

    velographx::UpdateBatch deletions;
    deletions.updates.reserve(local_end - local_begin);
    for (std::size_t i = local_begin; i < local_end; ++i) {
      const auto old_index = i - imported_edges;
      deletions.remove(static_cast<velographx::VertexId>(edges[old_index].first),
                       static_cast<velographx::VertexId>(edges[old_index].second));
    }
    bfs.apply(deletions);
    ++batches;
  }
  const auto end = std::chrono::steady_clock::now();

  const auto final_layers = layer_histogram(bfs.distances());
  const auto incremental_digest = distance_digest(bfs.distances());
  bfs.recompute();
  const auto reference_digest = distance_digest(bfs.distances());
  const bool correct = incremental_digest == reference_digest;

  const double wall_us = std::chrono::duration<double, std::micro>(end - begin).count();
  const double mean_us = batches ? wall_us / static_cast<double>(batches) : 0.0;

  std::cerr << "initial_layers=" << initial_layers << '\n';
  std::cerr << "final_layers=" << final_layers << '\n';
  std::cout << "{\"correct\":" << (correct ? "true" : "false")
            << ",\"batches\":" << batches
            << ",\"wall_us\":" << wall_us
            << ",\"wall_mean_us\":" << mean_us
            << ",\"distance_digest\":" << reference_digest << "}\n";
  return correct ? 0 : 3;
}
