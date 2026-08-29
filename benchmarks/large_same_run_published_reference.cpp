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
  if (!in) throw std::runtime_error("failed to open edge list");
  std::vector<Edge> edges;
  velographx::VertexId max_vertex = 0;
  bool saw = false;
  std::string line;
  while (std::getline(in, line)) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    std::istringstream row(line);
    velographx::VertexId u = 0, v = 0;
    if (!(row >> u >> v) || u == v) continue;
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

std::uint64_t edge_key(velographx::VertexId u, velographx::VertexId v) {
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
    if (selected.insert(edge_key(u, v)).second) batch.add(u, v);
  }
  if (batch.updates.size() != requested) throw std::runtime_error("could not generate updates");
  return batch;
}

long long ns(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: large_same_run_published_reference <edge-list> <trusted-initial-triangles> <fraction>\n";
    return 2;
  }
  const std::string dataset = argv[1];
  const auto trusted = static_cast<std::uint64_t>(std::stoull(argv[2]));
  const double fraction = std::stod(argv[3]);
  if (!(fraction > 0.0)) return 2;

  std::size_t vertices = 0;
  const auto edges = load_edges(dataset, vertices);
  if (edges.empty()) return 2;

  velographx::DynamicGraph graph(vertices, false);
  graph.bulk_load_edges(edges);
  velographx::IncrementalTriangleCount triangles(graph, trusted);
  const std::size_t requested = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(edges.size() * fraction)));
  const auto updates = make_updates(graph, requested, 0x4d595df4d0f33173ULL ^ requested);

  const auto vb = Clock::now();
  triangles.apply(updates);
  const auto ve = Clock::now();
  const auto vx = triangles.value();

  GoldenCounter golden(std::numeric_limits<int>::max());
  for (const auto& [u, v] : edges) golden.insert_edge(u, v, 0);
  const auto gb = Clock::now();
  long long timestamp = 1;
  for (const auto& op : updates.updates) golden.insert_edge(op.src, op.dst, timestamp++);
  const auto gu = Clock::now();
  const auto gq = Clock::now();
  const auto golden_value = golden.triangle_count();
  const auto ge = Clock::now();

  const auto fb = Clock::now();
  triangles.recompute();
  const auto fe = Clock::now();
  const auto full = triangles.value();
  const bool correct = vx == full && vx == static_cast<std::uint64_t>(golden_value);

  std::cout << "dataset,vertices,base_edges,fraction,requested_edges,velographx_update_ns,golden_update_ns,golden_query_ns,golden_answer_ready_ns,full_recompute_ns,velographx_triangles,golden_triangles,recomputed_triangles,correct\n";
  std::cout << dataset << ',' << vertices << ',' << edges.size() << ',' << fraction << ',' << requested << ','
            << ns(vb, ve) << ',' << ns(gb, gu) << ',' << ns(gq, ge) << ',' << (ns(gb, gu) + ns(gq, ge)) << ','
            << ns(fb, fe) << ',' << vx << ',' << golden_value << ',' << full << ',' << (correct ? 1 : 0) << '\n';
  return correct ? 0 : 1;
}
