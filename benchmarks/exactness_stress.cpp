#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using velographx::DynamicGraph;
using velographx::IncrementalBFS;
using velographx::IncrementalTriangleCount;
using velographx::UpdateBatch;
using velographx::VertexId;

constexpr std::size_t kVertices = 40;

std::vector<std::uint32_t> reference_bfs(const DynamicGraph& graph, VertexId source) {
  constexpr auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> distance(graph.vertex_count(), unreachable);
  std::queue<VertexId> q;
  distance[source] = 0;
  q.push(source);
  while (!q.empty()) {
    const auto u = q.front(); q.pop();
    for (auto v : graph.neighbors(u)) {
      if (distance[v] != unreachable) continue;
      distance[v] = distance[u] + 1;
      q.push(v);
    }
  }
  return distance;
}

std::uint64_t reference_triangles(const DynamicGraph& graph) {
  std::uint64_t triangles = 0;
  for (VertexId u = 0; u < graph.vertex_count(); ++u) {
    const auto nu = graph.neighbors(u);
    for (auto v : nu) {
      if (v <= u) continue;
      const auto nv = graph.neighbors(v);
      std::size_t i = 0, j = 0;
      while (i < nu.size() && j < nv.size()) {
        if (nu[i] == nv[j]) {
          if (nv[j] > v) ++triangles;
          ++i; ++j;
        } else if (nu[i] < nv[j]) ++i;
        else ++j;
      }
    }
  }
  return triangles;
}

std::vector<std::pair<VertexId, VertexId>> base_edges(const std::string& scenario) {
  std::vector<std::pair<VertexId, VertexId>> edges;
  if (scenario == "disconnected") {
    for (VertexId u = 0; u + 1 < 20; ++u) edges.emplace_back(u, u + 1);
    for (VertexId u = 20; u + 1 < 40; ++u) edges.emplace_back(u, u + 1);
  } else if (scenario == "hub") {
    for (VertexId v = 1; v < 40; ++v) edges.emplace_back(0, v);
  } else if (scenario == "path") {
    for (VertexId u = 0; u + 1 < 40; ++u) edges.emplace_back(u, u + 1);
  } else if (scenario == "dense") {
    for (VertexId u = 0; u < 20; ++u)
      for (VertexId v = u + 1; v < 20; ++v) edges.emplace_back(u, v);
    for (VertexId u = 20; u + 1 < 40; ++u) edges.emplace_back(u, u + 1);
  } else if (scenario == "destructive") {
    for (VertexId u = 0; u + 1 < 40; ++u) edges.emplace_back(u, u + 1);
    for (VertexId u = 0; u + 4 < 40; u += 2) edges.emplace_back(u, u + 4);
  } else {
    for (VertexId u = 0; u < 40; ++u) {
      edges.emplace_back(u, static_cast<VertexId>((u + 1) % 40));
      edges.emplace_back(u, static_cast<VertexId>((u + 7) % 40));
    }
  }
  return edges;
}

std::pair<VertexId, VertexId> choose_edge(const std::string& scenario, std::uint64_t step,
                                          std::mt19937& rng) {
  if (scenario == "hub") {
    return {0, static_cast<VertexId>(1 + (step * 17) % 39)};
  }
  if (scenario == "path") {
    const auto u = static_cast<VertexId>((step * 13) % 38);
    return {u, static_cast<VertexId>(u + 1 + (step % 2))};
  }
  if (scenario == "disconnected") {
    if (step % 31 == 0) return {19, 20};
    const VertexId base = (step % 2) ? 0 : 20;
    const auto u = static_cast<VertexId>(base + ((step * 11) % 18));
    return {u, static_cast<VertexId>(u + 1 + (step % 2))};
  }
  if (scenario == "dense") {
    const auto u = static_cast<VertexId>((step * 7) % 20);
    auto v = static_cast<VertexId>((step * 17 + 3) % 20);
    if (u == v) v = static_cast<VertexId>((v + 1) % 20);
    return {u, v};
  }
  if (scenario == "destructive") {
    if (step % 3 != 0) {
      const auto u = static_cast<VertexId>((step * 5) % 39);
      return {u, static_cast<VertexId>(u + 1)};
    }
    const auto u = static_cast<VertexId>((step * 9) % 36);
    return {u, static_cast<VertexId>(u + 4)};
  }
  std::uniform_int_distribution<std::uint32_t> dist(0, 39);
  VertexId u = dist(rng), v = dist(rng);
  if (u == v) v = static_cast<VertexId>((v + 1) % 40);
  return {u, v};
}

bool choose_add(const std::string& scenario, std::uint64_t step, std::mt19937& rng) {
  if (scenario == "destructive") return (step % 5) == 0;
  if (scenario == "hub" || scenario == "path" || scenario == "dense") return (step & 1U) != 0;
  std::bernoulli_distribution add(0.52);
  return add(rng);
}

int run_scenario(const std::string& scenario, std::size_t operations, std::size_t batch_size,
                 std::uint32_t seed) {
  DynamicGraph bfs_graph(kVertices, false);
  DynamicGraph triangle_graph(kVertices, false);
  const auto edges = base_edges(scenario);
  bfs_graph.bulk_load_edges(edges);
  triangle_graph.bulk_load_edges(edges);
  IncrementalBFS bfs(bfs_graph, 0);
  IncrementalTriangleCount triangles(triangle_graph);
  std::mt19937 rng(seed);

  std::size_t applied = 0;
  std::size_t batches = 0;
  while (applied < operations) {
    UpdateBatch bfs_batch, triangle_batch;
    const auto count = std::min(batch_size, operations - applied);
    bfs_batch.updates.reserve(count);
    triangle_batch.updates.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const auto step = static_cast<std::uint64_t>(applied + i);
      const auto [u, v] = choose_edge(scenario, step, rng);
      if (choose_add(scenario, step, rng)) {
        bfs_batch.add(u, v, step + 1);
        triangle_batch.add(u, v, step + 1);
      } else {
        bfs_batch.remove(u, v, step + 1);
        triangle_batch.remove(u, v, step + 1);
      }
    }
    bfs.apply(bfs_batch);
    triangles.apply(triangle_batch);
    applied += count;
    ++batches;

    if (bfs.distances() != reference_bfs(bfs_graph, 0)) {
      std::cerr << "BFS mismatch scenario=" << scenario << " batch=" << batches << '\n';
      return 2;
    }
    if (triangles.value() != reference_triangles(triangle_graph)) {
      std::cerr << "triangle mismatch scenario=" << scenario << " batch=" << batches << '\n';
      return 3;
    }
    if ((batches % 127) == 0) {
      bfs_graph.maybe_compact(0.0);
      triangle_graph.maybe_compact(0.0);
      if (bfs.distances() != reference_bfs(bfs_graph, 0) ||
          triangles.value() != reference_triangles(triangle_graph)) return 4;
    }
  }

  std::cout << "{\"scenario\":\"" << scenario << "\",\"operations\":" << operations
            << ",\"batches\":" << batches << ",\"bfs_mismatches\":0,\"triangle_mismatches\":0}\n";
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  const std::size_t total_operations = argc > 1 ? std::stoull(argv[1]) : 2000000;
  const std::size_t batch_size = argc > 2 ? std::stoull(argv[2]) : 256;
  const std::vector<std::string> scenarios{
      "random", "destructive", "disconnected", "hub", "path", "dense"};
  const auto each = (total_operations + scenarios.size() - 1) / scenarios.size();
  std::size_t total = 0;
  for (std::size_t i = 0; i < scenarios.size(); ++i) {
    const auto remaining = total_operations - total;
    const auto ops = std::min(each, remaining);
    if (ops == 0) break;
    const int rc = run_scenario(scenarios[i], ops, batch_size,
                                static_cast<std::uint32_t>(7919U * (i + 1)));
    if (rc != 0) return rc;
    total += ops;
  }
  std::cout << "{\"summary\":true,\"total_update_operations\":" << total
            << ",\"scenarios\":6,\"batch_size\":" << batch_size
            << ",\"mismatches\":0}\n";
  return total == total_operations ? 0 : 5;
}

// Validation harness is deterministic; this comment also lets helper-only fixes retrigger the campaign.
