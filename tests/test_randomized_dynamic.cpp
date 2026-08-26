#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

using velographx::DynamicGraph;
using velographx::UpdateBatch;
using velographx::VertexId;

std::vector<std::uint32_t> reference_bfs(const DynamicGraph& graph, VertexId source) {
  constexpr auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> distance(graph.vertex_count(), unreachable);
  if (source >= graph.vertex_count()) return distance;

  std::queue<VertexId> q;
  distance[source] = 0;
  q.push(source);
  while (!q.empty()) {
    const auto u = q.front();
    q.pop();
    for (const auto v : graph.neighbors(u)) {
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
    for (const auto v : nu) {
      if (v <= u) continue;
      const auto nv = graph.neighbors(v);
      std::size_t i = 0;
      std::size_t j = 0;
      while (i < nu.size() && j < nv.size()) {
        if (nu[i] == nv[j]) {
          if (nv[j] > v) ++triangles;
          ++i;
          ++j;
        } else if (nu[i] < nv[j]) {
          ++i;
        } else {
          ++j;
        }
      }
    }
  }
  return triangles;
}

void run_seed(std::uint32_t seed) {
  constexpr std::size_t vertices = 40;
  constexpr std::size_t operations = 2000;

  DynamicGraph bfs_graph(vertices, false);
  DynamicGraph triangle_graph(vertices, false);
  velographx::IncrementalBFS bfs(bfs_graph, 0);
  velographx::IncrementalTriangleCount triangles(triangle_graph);

  std::mt19937 rng(seed);
  std::uniform_int_distribution<std::uint32_t> vertex_dist(0, vertices - 1);
  std::bernoulli_distribution add_dist(0.58);

  for (std::size_t step = 0; step < operations; ++step) {
    VertexId u = vertex_dist(rng);
    VertexId v = vertex_dist(rng);
    if (u == v) v = static_cast<VertexId>((v + 1) % vertices);
    const bool add = add_dist(rng);

    UpdateBatch bfs_batch;
    UpdateBatch triangle_batch;
    if (add) {
      bfs_batch.add(u, v, step + 1);
      triangle_batch.add(u, v, step + 1);
    } else {
      bfs_batch.remove(u, v, step + 1);
      triangle_batch.remove(u, v, step + 1);
    }

    bfs.apply(bfs_batch);
    triangles.apply(triangle_batch);

    const auto expected_bfs = reference_bfs(bfs_graph, 0);
    assert(bfs.distances() == expected_bfs);
    assert(triangles.value() == reference_triangles(triangle_graph));

    if ((step % 127) == 0) {
      bfs_graph.maybe_compact(0.0);
      triangle_graph.maybe_compact(0.0);
      assert(bfs.distances() == reference_bfs(bfs_graph, 0));
      assert(triangles.value() == reference_triangles(triangle_graph));
    }
  }
}

}  // namespace

int main() {
  for (std::uint32_t seed = 1; seed <= 8; ++seed) run_seed(seed * 7919U);
  return 0;
}
