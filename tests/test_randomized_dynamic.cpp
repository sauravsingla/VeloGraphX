#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <random>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

using velographx::DynamicGraph;
using velographx::UpdateBatch;
using velographx::VertexId;

std::size_t env_size(const char* name, std::size_t fallback) {
  const char* value = std::getenv(name);
  if (!value || *value == '\0') return fallback;
  char* end = nullptr;
  const auto parsed = std::strtoull(value, &end, 10);
  if (end == value || *end != '\0' || parsed == 0) return fallback;
  return static_cast<std::size_t>(parsed);
}

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

std::vector<VertexId> reference_components(const DynamicGraph& graph) {
  const auto n = graph.vertex_count();
  const auto unset = std::numeric_limits<VertexId>::max();
  std::vector<VertexId> component(n, unset);
  VertexId cid = 0;
  std::queue<VertexId> q;
  for (VertexId start = 0; start < n; ++start) {
    if (component[start] != unset) continue;
    component[start] = cid;
    q.push(start);
    while (!q.empty()) {
      const auto u = q.front();
      q.pop();
      for (const auto v : graph.neighbors(u)) {
        if (component[v] != unset) continue;
        component[v] = cid;
        q.push(v);
      }
    }
    ++cid;
  }
  return component;
}

std::vector<std::uint32_t> reference_kcore(const DynamicGraph& graph) {
  const auto n = graph.vertex_count();
  std::vector<std::uint32_t> core(n, 0);
  for (std::uint32_t k = 1; k <= n; ++k) {
    std::vector<std::uint32_t> degree(n, 0);
    std::vector<std::uint8_t> removed(n, 0);
    std::queue<VertexId> q;
    for (VertexId u = 0; u < n; ++u) degree[u] = graph.neighbors(u).size();
    for (VertexId u = 0; u < n; ++u) {
      if (degree[u] < k) q.push(u);
    }
    while (!q.empty()) {
      const auto u = q.front();
      q.pop();
      if (removed[u] || degree[u] >= k) continue;
      removed[u] = 1;
      for (const auto v : graph.neighbors(u)) {
        if (removed[v]) continue;
        if (degree[v] != 0) --degree[v];
        if (degree[v] < k) q.push(v);
      }
    }
    for (VertexId u = 0; u < n; ++u) {
      if (!removed[u]) core[u] = k;
    }
  }
  return core;
}

void assert_same_partition(velographx::IncrementalComponents& incremental,
                           const std::vector<VertexId>& reference) {
  for (VertexId u = 0; u < reference.size(); ++u) {
    for (VertexId v = 0; v < reference.size(); ++v) {
      const bool expected = reference[u] == reference[v];
      const bool actual = incremental.component(u) == incremental.component(v);
      assert(actual == expected);
    }
  }
}

void run_seed(std::uint32_t seed, std::size_t operations) {
  constexpr std::size_t vertices = 40;

  DynamicGraph bfs_graph(vertices, false);
  DynamicGraph triangle_graph(vertices, false);
  DynamicGraph component_graph(vertices, false);
  DynamicGraph kcore_graph(vertices, false);
  velographx::IncrementalBFS bfs(bfs_graph, 0);
  velographx::IncrementalTriangleCount triangles(triangle_graph);
  velographx::IncrementalComponents components(component_graph);
  velographx::IncrementalKCore kcore(kcore_graph);

  std::mt19937 rng(seed);
  std::uniform_int_distribution<std::uint32_t> vertex_dist(0, vertices - 1);
  std::bernoulli_distribution add_dist(0.58);

  for (std::size_t step = 0; step < operations; ++step) {
    VertexId u = vertex_dist(rng);
    VertexId v = vertex_dist(rng);
    if (u == v) v = static_cast<VertexId>((v + 1) % vertices);
    const bool add = add_dist(rng);

    UpdateBatch batch;
    if (add) batch.add(u, v, step + 1);
    else batch.remove(u, v, step + 1);

    bfs.apply(batch);
    triangles.apply(batch);
    components.apply(batch);
    kcore.apply(batch);

    const auto expected_bfs = reference_bfs(bfs_graph, 0);
    assert(bfs.distances() == expected_bfs);
    assert(triangles.value() == reference_triangles(triangle_graph));

    if ((step % 17) == 0 || step + 1 == operations) {
      assert_same_partition(components, reference_components(component_graph));
      assert(kcore.core() == reference_kcore(kcore_graph));
    }

    if ((step % 127) == 0) {
      bfs_graph.maybe_compact(0.0);
      triangle_graph.maybe_compact(0.0);
      component_graph.maybe_compact(0.0);
      kcore_graph.maybe_compact(0.0);
      assert(bfs.distances() == reference_bfs(bfs_graph, 0));
      assert(triangles.value() == reference_triangles(triangle_graph));
      assert_same_partition(components, reference_components(component_graph));
      assert(kcore.core() == reference_kcore(kcore_graph));
    }
  }
}

}  // namespace

int main() {
  const auto operations = env_size("VELOGRAPHX_RANDOMIZED_OPS", 2000);
  const auto seeds = env_size("VELOGRAPHX_RANDOMIZED_SEEDS", 8);
  for (std::size_t seed = 1; seed <= seeds; ++seed) {
    run_seed(static_cast<std::uint32_t>(seed * 7919U), operations);
  }
  return 0;
}
