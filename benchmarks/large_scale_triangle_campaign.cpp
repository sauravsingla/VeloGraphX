#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
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
  velographx::VertexId max_vertex = 0;
  bool saw = false;
  std::string line;
  while (std::getline(in, line)) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    std::istringstream row(line);
    velographx::VertexId u = 0, v = 0;
    if (!(row >> u >> v)) continue;
    if (u == v) continue;
    if (u > v) std::swap(u, v);
    edges.emplace_back(u, v);
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw = true;
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  vertex_count = saw ? static_cast<std::size_t>(max_vertex) + 1 : 0;
  return edges;
}

std::vector<double> parse_fractions(const std::string& value) {
  std::vector<double> out;
  std::istringstream input(value);
  std::string token;
  while (std::getline(input, token, ',')) {
    const double v = std::stod(token);
    if (!(v > 0.0) || !std::isfinite(v)) throw std::runtime_error("invalid fraction");
    out.push_back(v);
  }
  return out;
}

std::uint64_t key(velographx::VertexId u, velographx::VertexId v) {
  if (u > v) std::swap(u, v);
  return (static_cast<std::uint64_t>(u) << 32U) | v;
}

velographx::UpdateBatch make_updates(const velographx::DynamicGraph& graph,
                                     std::size_t requested,
                                     std::uint64_t seed) {
  velographx::UpdateBatch batch;
  batch.updates.reserve(requested);
  std::unordered_set<std::uint64_t> selected;
  selected.reserve(requested * 2 + 1);
  const auto n = static_cast<std::uint64_t>(graph.vertex_count());
  std::uint64_t state = seed | 1ULL;
  std::size_t attempts = 0;
  const std::size_t max_attempts = std::max<std::size_t>(requested * 500, 100000);
  while (batch.updates.size() < requested && attempts++ < max_attempts) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    const auto u = static_cast<velographx::VertexId>((state >> 17) % n);
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    const auto v = static_cast<velographx::VertexId>((state >> 17) % n);
    if (u == v || graph.has_edge(u, v)) continue;
    if (selected.insert(key(u, v)).second) batch.add(u, v);
  }
  if (batch.updates.size() != requested) throw std::runtime_error("could not generate updates");
  return batch;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 5) {
    std::cerr << "usage: velographx_large_scale_triangle_benchmark <edge-list> <trusted-initial-triangles> [repeats] [fractions-csv]\n";
    return 2;
  }
  const std::string dataset = argv[1];
  const auto trusted = static_cast<std::uint64_t>(std::stoull(argv[2]));
  const int repeats = argc >= 4 ? std::stoi(argv[3]) : 1;
  const auto fractions = argc == 5 ? parse_fractions(argv[4]) : std::vector<double>{0.00001, 0.0001, 0.001, 0.01};

  std::size_t vertices = 0;
  const auto edges = load_edges(dataset, vertices);
  if (edges.empty()) return 2;

  using clock = std::chrono::steady_clock;
  std::cout << "dataset,vertices,base_edges,fraction,requested_edges,repeat,incremental_ns,full_recompute_ns,speedup,incremental_triangles,recomputed_triangles,correct\n";

  for (double fraction : fractions) {
    const auto requested = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(edges.size() * fraction)));
    for (int repeat = 0; repeat < repeats; ++repeat) {
      velographx::DynamicGraph graph(vertices, false);
      graph.bulk_load_edges(edges);
      velographx::IncrementalTriangleCount triangles(graph, trusted);
      const auto updates = make_updates(graph, requested,
          0x9e3779b97f4a7c15ULL ^ static_cast<std::uint64_t>(requested) ^ static_cast<std::uint64_t>(repeat + 1));

      const auto ib = clock::now();
      triangles.apply(updates);
      const auto ie = clock::now();
      const auto incremental_value = triangles.value();

      const auto fb = clock::now();
      triangles.recompute();
      const auto fe = clock::now();
      const auto full_value = triangles.value();

      const auto ins = std::chrono::duration_cast<std::chrono::nanoseconds>(ie - ib).count();
      const auto fns = std::chrono::duration_cast<std::chrono::nanoseconds>(fe - fb).count();
      const bool correct = incremental_value == full_value;
      const double speedup = ins > 0 ? static_cast<double>(fns) / static_cast<double>(ins)
                                     : std::numeric_limits<double>::quiet_NaN();
      std::cout << dataset << ',' << vertices << ',' << edges.size() << ',' << fraction << ',' << requested << ','
                << repeat << ',' << ins << ',' << fns << ',' << speedup << ',' << incremental_value << ','
                << full_value << ',' << (correct ? 1 : 0) << '\n';
      if (!correct) return 1;
    }
  }
  return 0;
}
