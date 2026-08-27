#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

using Edge = std::pair<velographx::VertexId, velographx::VertexId>;

std::vector<Edge> load_edges(const std::string& path, std::size_t& vertex_count) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("failed to open edge list: " + path);

  std::vector<Edge> edges;
  velographx::VertexId u = 0;
  velographx::VertexId v = 0;
  velographx::VertexId max_vertex = 0;
  bool saw_edge = false;
  while (in >> u >> v) {
    if (u == v) continue;
    if (u > v) std::swap(u, v);
    edges.emplace_back(u, v);
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw_edge = true;
  }
  if (!in.eof()) throw std::runtime_error("malformed edge list: " + path);
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  vertex_count = saw_edge ? static_cast<std::size_t>(max_vertex) + 1 : 0;
  return edges;
}

velographx::DynamicGraph make_graph(std::size_t vertex_count, const std::vector<Edge>& edges) {
  velographx::DynamicGraph graph(vertex_count, false);
  velographx::UpdateBatch seed;
  seed.updates.reserve(edges.size());
  for (const auto& [u, v] : edges) seed.add(u, v);
  graph.apply(seed);
  graph.compact();
  return graph;
}

velographx::UpdateBatch make_updates(
    const velographx::DynamicGraph& graph,
    std::size_t requested,
    std::uint64_t seed) {
  velographx::UpdateBatch batch;
  batch.updates.reserve(requested);
  if (graph.vertex_count() < 2 || requested == 0) return batch;

  const auto n = static_cast<std::uint64_t>(graph.vertex_count());
  std::uint64_t state = seed | 1ULL;
  const std::size_t max_attempts = std::max<std::size_t>(requested * 200, 10000);
  std::size_t attempts = 0;
  while (batch.updates.size() < requested && attempts++ < max_attempts) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    const auto u = static_cast<velographx::VertexId>((state >> 17) % n);
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    const auto v = static_cast<velographx::VertexId>((state >> 17) % n);
    if (u == v || graph.has_edge(u, v)) continue;

    bool duplicate = false;
    for (const auto& op : batch.updates) {
      if ((op.src == u && op.dst == v) || (op.src == v && op.dst == u)) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) batch.add(u, v);
  }

  if (batch.updates.size() != requested) {
    throw std::runtime_error("could not generate requested number of missing-edge updates");
  }
  return batch;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: velographx_public_update_fraction_benchmark <edge-list> [repeats]\n";
    return 2;
  }

  const std::string dataset = argv[1];
  const int repeats = argc == 3 ? std::stoi(argv[2]) : 10;
  if (repeats <= 0) {
    std::cerr << "repeats must be positive\n";
    return 2;
  }

  std::size_t vertex_count = 0;
  const auto edges = load_edges(dataset, vertex_count);
  if (edges.empty()) {
    std::cerr << "dataset has no edges\n";
    return 2;
  }

  constexpr std::array<double, 7> fractions = {
      0.000001, 0.00001, 0.0001, 0.001, 0.01, 0.05, 0.10};
  using clock = std::chrono::steady_clock;

  std::cout << "dataset,algorithm,vertices,base_edges,update_fraction,requested_edges,changed_edges,repeat,incremental_ns,full_recompute_ns,speedup,incremental_triangles,recomputed_triangles,correct\n";

  for (double fraction : fractions) {
    const std::size_t requested = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(static_cast<double>(edges.size()) * fraction)));

    for (int repeat = 0; repeat < repeats; ++repeat) {
      auto graph = make_graph(vertex_count, edges);
      velographx::IncrementalTriangleCount triangles(graph);
      const auto update = make_updates(
          graph,
          requested,
          0x9e3779b97f4a7c15ULL ^
              (static_cast<std::uint64_t>(repeat + 1) * 0xbf58476d1ce4e5b9ULL) ^
              static_cast<std::uint64_t>(requested));

      const auto incremental_begin = clock::now();
      triangles.apply(update);
      const auto incremental_end = clock::now();
      const auto incremental_value = triangles.value();

      const auto full_begin = clock::now();
      triangles.recompute();
      const auto full_end = clock::now();
      const auto recomputed_value = triangles.value();

      const auto incremental_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
          incremental_end - incremental_begin).count();
      const auto full_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
          full_end - full_begin).count();
      const double speedup = incremental_ns > 0
          ? static_cast<double>(full_ns) / static_cast<double>(incremental_ns)
          : std::numeric_limits<double>::quiet_NaN();
      const bool correct = incremental_value == recomputed_value;

      std::cout << dataset << ",triangle_count," << vertex_count << ',' << edges.size() << ','
                << fraction << ',' << requested << ',' << update.updates.size() << ',' << repeat << ','
                << incremental_ns << ',' << full_ns << ',' << speedup << ','
                << incremental_value << ',' << recomputed_value << ',' << (correct ? 1 : 0) << '\n';

      if (!correct) {
        std::cerr << "incremental triangle count disagrees with full recomputation\n";
        return 1;
      }
    }
  }
  return 0;
}
