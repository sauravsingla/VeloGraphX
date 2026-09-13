#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "velographx/algorithms.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/pagerank.hpp"
#include "velographx/incremental/sssp.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

int main() {
  using namespace velographx;

  // Existing smoke coverage.
  DynamicGraph g(5, false);
  UpdateBatch b;
  b.add(0, 1);
  b.add(1, 2);
  g.apply(b);

  IncrementalBFS bfs(g, 0);
  assert(bfs.distances()[2] == 2);
  UpdateBatch c;
  c.add(2, 3);
  bfs.apply(c);
  assert(bfs.distances()[3] == 3);
  IncrementalSSSP s(g, 0);
  assert(s.distances()[3] == 3);
  IncrementalKCore kc(g);
  assert(kc.core().size() == 5);
  IncrementalPageRank pr(g);
  assert(pr.values().size() == 5);

  // Later additions in one batch must observe earlier additions.
  DynamicGraph tg(3, false);
  tg.add_edge(0, 1);
  IncrementalTriangleCount triangles(tg);
  const auto before_version = tg.version();
  UpdateBatch triangle_batch;
  triangle_batch.add(1, 2);
  triangle_batch.add(0, 2);
  triangles.apply(triangle_batch);
  assert(triangles.value() == 1);
  assert(tg.version() == before_version + 1);
  triangles.recompute();
  assert(triangles.value() == 1);

  // k-core regression: a vertex whose degree drops from 3 to 2 during peeling
  // belongs to the 2-core, not the 3-core.
  {
    DynamicGraph graph(4, false);
    graph.bulk_load_edges({{0, 1}, {0, 2}, {0, 3}, {1, 2}});
    IncrementalKCore kcore(graph);
    const std::vector<std::uint32_t> expected{2, 2, 2, 1};
    assert(kcore.core() == expected);
  }

  // Connected-components regression: add then remove in one batch must not
  // leave union-find connected when the final graph has no edge.
  {
    DynamicGraph graph(2, false);
    IncrementalComponents components(graph);
    UpdateBatch batch;
    batch.add(0, 1);
    batch.remove(0, 1);
    components.apply(batch);
    assert(!graph.has_edge(0, 1));
    assert(components.component(0) != components.component(1));

    bool out_of_range = false;
    try {
      (void)components.component(2);
    } catch (const std::out_of_range&) {
      out_of_range = true;
    }
    assert(out_of_range);
  }

  // Reversed endpoints are the same logical undirected edge. Final add wins.
  {
    DynamicGraph graph(2, false);
    graph.add_edge(0, 1);
    IncrementalComponents components(graph);
    UpdateBatch batch;
    batch.remove(0, 1);
    batch.add(1, 0);
    components.apply(batch);
    assert(graph.has_edge(0, 1));
    assert(components.component(0) == components.component(1));
  }

  // Directed connectivity semantics are rejected until weak/strong semantics
  // are requested explicitly.
  {
    bool static_rejected = false;
    try {
      CsrGraph graph({{1, 0}}, true);
      (void)connected_components(graph);
    } catch (const std::invalid_argument&) {
      static_rejected = true;
    }
    assert(static_rejected);

    bool incremental_rejected = false;
    try {
      DynamicGraph graph(2, true);
      IncrementalComponents components(graph);
      (void)components;
    } catch (const std::invalid_argument&) {
      incremental_rejected = true;
    }
    assert(incremental_rejected);
  }

  // Directed triangle counting is rejected instead of returning an
  // ID-order-dependent quantity.
  {
    bool rejected = false;
    try {
      DynamicGraph graph(3, true);
      IncrementalTriangleCount directed_triangles(graph);
      (void)directed_triangles;
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  // Multi-operation triangle maintenance must see earlier operations while the
  // underlying graph still advances by one logical batch version.
  {
    DynamicGraph graph(3, false);
    graph.bulk_load_edges({{0, 1}, {1, 2}});
    IncrementalTriangleCount incremental(graph);
    const auto version_before = graph.version();
    UpdateBatch batch;
    batch.add(0, 2);
    batch.remove(1, 2);
    incremental.apply(batch);
    assert(incremental.value() == 0);
    assert(graph.version() == version_before + 1);
    assert(graph.has_edge(0, 2));
    assert(!graph.has_edge(1, 2));
  }

  // Weighted SSSP regression: 10 -> 5 -> 8 in one batch must use final weight 8.
  {
    WeightedDynamicGraph graph(2, true);
    WeightedUpdateBatch seed;
    seed.add(0, 1, 10);
    graph.apply(seed);
    IncrementalWeightedSSSP sssp(graph, 0);
    assert(sssp.distances()[1] == 10);

    WeightedUpdateBatch batch;
    batch.update(0, 1, 5);
    batch.update(0, 1, 8);
    sssp.apply(batch);
    assert(graph.weight(0, 1).has_value() && *graph.weight(0, 1) == 8);
    assert(sssp.distances()[1] == 8);

    const auto version_before_invalid = graph.version();
    bool rejected = false;
    try {
      WeightedUpdateBatch invalid;
      invalid.update(0, 1, kMaxFiniteWeightedDistance + 1);
      graph.apply(invalid);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
    assert(graph.version() == version_before_invalid);
    assert(graph.weight(0, 1).has_value() && *graph.weight(0, 1) == 8);
  }

  // Weighted storage follows simple-graph semantics for self-loops.
  {
    WeightedDynamicGraph graph(2, false);
    WeightedUpdateBatch batch;
    batch.add(0, 0, 1);
    graph.apply(batch);
    assert(!graph.weight(0, 0).has_value());
  }

  // Pre-existing dangling PageRank mass is global, so any structural update
  // takes the exact full-solve fallback.
  {
    DynamicGraph graph(5, true);
    graph.bulk_load_edges({{0, 1}, {1, 0}, {2, 3}, {3, 2}});  // vertex 4 dangling
    IncrementalPageRank pagerank(graph);
    UpdateBatch batch;
    batch.add(0, 2);
    pagerank.apply(batch);
    assert(pagerank.last_repaired_vertices() == graph.vertex_count());
    assert(pagerank.validate_against_full().within_tolerance);
  }

  // Growing n changes teleportation globally even without dangling vertices.
  {
    DynamicGraph graph(2, false);
    graph.add_edge(0, 1);
    IncrementalPageRank pagerank(graph);
    UpdateBatch batch;
    batch.add(2, 3);
    pagerank.apply(batch);
    assert(graph.vertex_count() == 4);
    assert(pagerank.last_repaired_vertices() == 4);
    assert(pagerank.validate_against_full().within_tolerance);
  }

  // Static CSR construction follows simple-graph semantics for loops.
  {
    CsrGraph graph({{0, 0}, {0, 1}}, false);
    assert(!graph.has_edge(0, 0));
    assert(graph.has_edge(0, 1));
  }

  return 0;
}
