#include <cstdint>
#include <iterator>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

using velographx::DynamicGraph;
using velographx::IncrementalBFS;
using velographx::UpdateBatch;
using velographx::VertexId;

static void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

static void assert_matches_full(DynamicGraph& graph, IncrementalBFS& bfs,
                                VertexId source, const std::string& context) {
  IncrementalBFS full(graph, source);
  if (bfs.distances() == full.distances()) return;
  const auto& got = bfs.distances();
  const auto& expected = full.distances();
  const auto n = std::min(got.size(), expected.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (got[i] != expected[i]) {
      throw std::runtime_error(context + ": BFS mismatch at vertex " + std::to_string(i) +
                               ", got=" + std::to_string(got[i]) +
                               ", expected=" + std::to_string(expected[i]));
    }
  }
  throw std::runtime_error(context + ": BFS distance-vector size mismatch");
}

int main() {
  // Removing one parent conservatively invalidates the downstream old-DAG
  // region, then reconstructs the same distances from the alternate parent.
  {
    DynamicGraph g(5, true);
    g.bulk_load_edges({{0,1},{0,2},{1,3},{2,3},{3,4}});
    IncrementalBFS bfs(g, 0);
    UpdateBatch b;
    b.remove(1,3);
    bfs.apply(b);
    require(bfs.distances()[3] == 2, "alternate-parent: distance(3)");
    require(bfs.distances()[4] == 3, "alternate-parent: distance(4)");
    require(bfs.last_affected_vertices() == 2, "alternate-parent: affected count");
    require(!bfs.last_used_full_recompute(), "alternate-parent: unexpected fallback");
    assert_matches_full(g, bfs, 0, "alternate-parent");
  }

  // Deleting one shortest-path branch must preserve a surviving equal-length branch.
  {
    DynamicGraph g(7, true);
    g.bulk_load_edges({{0,1},{1,2},{2,3},{3,4},{0,5},{5,6},{6,3}});
    IncrementalBFS bfs(g, 0);
    require(bfs.distances()[3] == 3, "surviving-branch: initial distance(3)");
    UpdateBatch b;
    b.remove(1,2);
    bfs.apply(b);
    require(bfs.distances()[2] == IncrementalBFS::unreachable,
            "surviving-branch: distance(2)");
    require(bfs.distances()[3] == 3, "surviving-branch: distance(3)");
    require(bfs.distances()[4] == 4, "surviving-branch: distance(4)");
    require(!bfs.last_used_full_recompute(), "surviving-branch: unexpected fallback");
    assert_matches_full(g, bfs, 0, "surviving-branch");
  }

  // Mixed delete+insert: invalidate the old branch, then allow the new edge to
  // create a shorter replacement path in the same batch.
  {
    DynamicGraph g(6, true);
    g.bulk_load_edges({{0,1},{1,2},{2,3},{3,4},{0,5}});
    IncrementalBFS bfs(g, 0);
    UpdateBatch b;
    b.remove(1,2);
    b.add(5,3);
    bfs.apply(b);
    require(bfs.distances()[2] == IncrementalBFS::unreachable, "mixed: distance(2)");
    require(bfs.distances()[3] == 2, "mixed: distance(3)");
    require(bfs.distances()[4] == 3, "mixed: distance(4)");
    assert_matches_full(g, bfs, 0, "mixed");
  }

  // Undirected deletion repair must account for both orientations.
  {
    DynamicGraph g(5, false);
    g.bulk_load_edges({{0,1},{1,2},{2,3},{0,4},{4,3}});
    IncrementalBFS bfs(g, 0);
    UpdateBatch b;
    b.remove(1,2);
    bfs.apply(b);
    require(bfs.distances()[2] == 3, "undirected: distance(2)");
    assert_matches_full(g, bfs, 0, "undirected");
  }

  // A tiny fallback budget must fail closed to a full recomputation.
  {
    DynamicGraph g(20, true);
    std::vector<std::pair<VertexId,VertexId>> edges;
    for (VertexId i=0;i+1<20;++i) edges.emplace_back(i,i+1);
    g.bulk_load_edges(edges);
    IncrementalBFS bfs(g, 0, 0.05);
    UpdateBatch b;
    b.remove(0,1);
    bfs.apply(b);
    require(bfs.last_used_full_recompute(), "fallback: full recompute not used");
    assert_matches_full(g, bfs, 0, "fallback");
  }

  // Deterministic randomized differential test across mixed additions and
  // deletions. Every batch is compared with a fresh full BFS rebuild.
  {
    constexpr VertexId n = 48;
    DynamicGraph g(n, true);
    std::set<std::pair<VertexId,VertexId>> live;
    std::mt19937 rng(20260830u);
    std::uniform_int_distribution<VertexId> vertex(0, n-1);
    while (live.size() < 180) {
      auto u = vertex(rng), v = vertex(rng);
      if (u != v) live.emplace(u,v);
    }
    std::vector<std::pair<VertexId,VertexId>> initial(live.begin(), live.end());
    g.bulk_load_edges(initial);
    IncrementalBFS bfs(g, 0, 0.75);

    for (int epoch=0; epoch<250; ++epoch) {
      UpdateBatch batch;
      for (int j=0; j<6; ++j) {
        const bool do_add = live.empty() || ((rng() & 1u) == 0u);
        if (do_add) {
          for (int tries=0; tries<30; ++tries) {
            auto u = vertex(rng), v = vertex(rng);
            if (u == v || live.contains({u,v})) continue;
            live.emplace(u,v);
            batch.add(u,v);
            break;
          }
        } else {
          const auto offset = static_cast<std::size_t>(rng() % live.size());
          auto it = live.begin();
          std::advance(it, static_cast<long>(offset));
          const auto [u,v] = *it;
          live.erase(it);
          batch.remove(u,v);
        }
      }
      bfs.apply(batch);
      assert_matches_full(g, bfs, 0, "randomized epoch " + std::to_string(epoch));
    }
  }
}
