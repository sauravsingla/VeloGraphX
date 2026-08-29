#include <cassert>
#include <cstdint>
#include <iterator>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

using velographx::DynamicGraph;
using velographx::IncrementalBFS;
using velographx::UpdateBatch;
using velographx::VertexId;

static void assert_matches_full(DynamicGraph& graph, IncrementalBFS& bfs, VertexId source) {
  IncrementalBFS full(graph, source);
  assert(bfs.distances() == full.distances());
}

int main() {
  // Removing one parent must preserve a vertex with an alternate shortest parent.
  {
    DynamicGraph g(5, true);
    g.bulk_load_edges({{0,1},{0,2},{1,3},{2,3},{3,4}});
    IncrementalBFS bfs(g, 0);
    UpdateBatch b;
    b.remove(1,3);
    bfs.apply(b);
    assert(bfs.distances()[3] == 2);
    assert(bfs.distances()[4] == 3);
    assert(bfs.last_affected_vertices() == 0);
    assert(!bfs.last_used_full_recompute());
    assert_matches_full(g, bfs, 0);
  }

  // Deleting the only tree support invalidates a descendant region and repairs
  // it from a longer surviving boundary path.
  {
    DynamicGraph g(7, true);
    g.bulk_load_edges({{0,1},{1,2},{2,3},{3,4},{0,5},{5,6},{6,3}});
    IncrementalBFS bfs(g, 0);
    assert(bfs.distances()[3] == 3);
    UpdateBatch b;
    b.remove(1,2);
    bfs.apply(b);
    assert(bfs.distances()[2] == IncrementalBFS::unreachable);
    assert(bfs.distances()[3] == 3); // alternate 0-5-6-3 remains shortest
    assert(bfs.distances()[4] == 4);
    assert(!bfs.last_used_full_recompute());
    assert_matches_full(g, bfs, 0);
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
    assert(bfs.distances()[2] == IncrementalBFS::unreachable);
    assert(bfs.distances()[3] == 2);
    assert(bfs.distances()[4] == 3);
    assert_matches_full(g, bfs, 0);
  }

  // Undirected deletion repair must account for both orientations.
  {
    DynamicGraph g(5, false);
    g.bulk_load_edges({{0,1},{1,2},{2,3},{0,4},{4,3}});
    IncrementalBFS bfs(g, 0);
    UpdateBatch b;
    b.remove(1,2);
    bfs.apply(b);
    assert(bfs.distances()[2] == 3); // 0-4-3-2
    assert_matches_full(g, bfs, 0);
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
    assert(bfs.last_used_full_recompute());
    assert_matches_full(g, bfs, 0);
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
      assert_matches_full(g, bfs, 0);
    }
  }
}
