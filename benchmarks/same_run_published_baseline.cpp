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
#include "GoldenCounter.h"

namespace {
using Edge = std::pair<velographx::VertexId, velographx::VertexId>;
using Clock = std::chrono::steady_clock;

std::vector<Edge> load_edges(const std::string& path, std::size_t& vertex_count) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("failed to open edge list: " + path);
  std::vector<Edge> edges;
  velographx::VertexId max_vertex = 0;
  bool saw_edge = false;
  std::string line;
  while (std::getline(in, line)) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    std::istringstream row(line);
    velographx::VertexId u = 0, v = 0;
    if (!(row >> u >> v)) throw std::runtime_error("malformed edge-list row");
    if (u == v) continue;
    if (u > v) std::swap(u, v);
    edges.emplace_back(u, v);
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw_edge = true;
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  vertex_count = saw_edge ? static_cast<std::size_t>(max_vertex) + 1 : 0;
  return edges;
}

std::uint64_t edge_key(velographx::VertexId u, velographx::VertexId v) {
  if (u > v) std::swap(u, v);
  return (static_cast<std::uint64_t>(u) << 32U) | static_cast<std::uint64_t>(v);
}

velographx::DynamicGraph make_velographx_graph(std::size_t n, const std::vector<Edge>& edges) {
  velographx::DynamicGraph graph(n, false);
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
  std::unordered_set<std::uint64_t> selected;
  selected.reserve(requested * 2 + 1);
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
    const auto key = edge_key(u, v);
    if (selected.insert(key).second) batch.add(u, v);
  }
  if (batch.updates.size() != requested) {
    throw std::runtime_error("could not generate requested missing-edge batch");
  }
  return batch;
}

void seed_golden(GoldenCounter& golden, const std::vector<Edge>& edges) {
  for (const auto& [u, v] : edges) golden.insert_edge(u, v, 0);
}

long long ns(Clock::time_point begin, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) {
    std::cerr << "usage: same_run_published_baseline <edge-list> [repeats] [fractions-csv]\n";
    return 2;
  }
  const std::string dataset = argv[1];
  const int repeats = argc >= 3 ? std::stoi(argv[2]) : 5;
  if (repeats <= 0) return 2;

  std::vector<double> fractions{0.01, 0.05, 0.10};
  if (argc == 4) {
    fractions.clear();
    std::istringstream input(argv[3]);
    std::string token;
    while (std::getline(input, token, ',')) {
      const double f = std::stod(token);
      if (!(f > 0.0) || !std::isfinite(f)) return 2;
      fractions.push_back(f);
    }
  }

  std::size_t vertex_count = 0;
  const auto edges = load_edges(dataset, vertex_count);
  if (edges.empty()) return 2;

  std::cout << "dataset,vertices,base_edges,update_fraction,requested_edges,repeat,"
               "velographx_update_ns,velographx_full_recompute_ns,velographx_speedup,"
               "golden_update_only_ns,golden_exact_query_ns,golden_answer_ready_ns,"
               "velographx_updates_per_s,golden_answer_ready_updates_per_s,"
               "velographx_triangles,golden_triangles,recomputed_triangles,correct\n";

  for (double fraction : fractions) {
    const std::size_t requested = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(static_cast<double>(edges.size()) * fraction)));

    for (int repeat = 0; repeat < repeats; ++repeat) {
      auto graph = make_velographx_graph(vertex_count, edges);
      velographx::IncrementalTriangleCount triangles(graph);
      const auto update = make_updates(
          graph,
          requested,
          0x9e3779b97f4a7c15ULL ^
              (static_cast<std::uint64_t>(repeat + 1) * 0xbf58476d1ce4e5b9ULL) ^
              static_cast<std::uint64_t>(requested));

      const auto vx_begin = Clock::now();
      triangles.apply(update);
      const auto vx_end = Clock::now();
      const auto vx_value = triangles.value();

      const auto full_begin = Clock::now();
      triangles.recompute();
      const auto full_end = Clock::now();
      const auto full_value = triangles.value();

      GoldenCounter golden(std::numeric_limits<int>::max());
      seed_golden(golden, edges);
      const auto golden_update_begin = Clock::now();
      long long timestamp = 1;
      for (const auto& op : update.updates) {
        golden.insert_edge(op.src, op.dst, timestamp++);
      }
      const auto golden_update_end = Clock::now();
      const auto golden_query_begin = Clock::now();
      const auto golden_value = golden.triangle_count();
      const auto golden_query_end = Clock::now();

      const auto vx_ns = ns(vx_begin, vx_end);
      const auto full_ns = ns(full_begin, full_end);
      const auto golden_update_ns = ns(golden_update_begin, golden_update_end);
      const auto golden_query_ns = ns(golden_query_begin, golden_query_end);
      const auto golden_ready_ns = golden_update_ns + golden_query_ns;
      const bool correct =
          static_cast<std::uint64_t>(golden_value) == vx_value && vx_value == full_value;
      if (!correct) {
        std::cerr << "exact-count disagreement: VeloGraphX=" << vx_value
                  << " GoldenCounter=" << golden_value << " full=" << full_value << '\n';
        return 1;
      }

      const double vx_speedup = vx_ns > 0 ? static_cast<double>(full_ns) / vx_ns : 0.0;
      const double vx_rate = vx_ns > 0 ? static_cast<double>(requested) * 1e9 / vx_ns : 0.0;
      const double golden_rate = golden_ready_ns > 0
          ? static_cast<double>(requested) * 1e9 / golden_ready_ns : 0.0;

      std::cout << dataset << ',' << vertex_count << ',' << edges.size() << ',' << fraction << ','
                << requested << ',' << repeat << ',' << vx_ns << ',' << full_ns << ',' << vx_speedup
                << ',' << golden_update_ns << ',' << golden_query_ns << ',' << golden_ready_ns << ','
                << vx_rate << ',' << golden_rate << ',' << vx_value << ',' << golden_value << ','
                << full_value << ',' << (correct ? 1 : 0) << '\n';
    }
  }
  return 0;
}
