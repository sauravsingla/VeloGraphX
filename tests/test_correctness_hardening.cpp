#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "velographx/algorithms.hpp"
#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/pagerank.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

int main() {
  using namespace velographx;

  // k-core regression: initial-degree bucket implementations can incorrectly
  // leave vertex 0 in core 3 after vertex 3 is peeled.
  {
    DynamicGraph graph(4, false);
    graph.bulk_load_edges({{0, 1}, {0, 2}, {0, 3}, {1, 2}});
    IncrementalKCore kcore(graph);
    const std::vector<std::uint32_t> expected{2, 2, 2, 1};
    assert(kcore.core() == expected);
  }

  // Connected-components regression: add then remove in one batch must not
  // leave the union-find state connected when the final graph has no edge.
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

  // Reversed endpoints are the same logical undirected edge. The final add
  // must win and connectivity must match the graph.
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

  // Directed connectivity semantics are intentionally rejected until weak or
  // strong connectivity is requested explicitly.
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

  // Directed triangle counting is likewise rejected instead of returning an
  // ID-order-dependent quantity.
  {
    bool rejected = false;
    try {
      DynamicGraph graph(3, true);
      IncrementalTriangleCount triangles(graph);
      (void)triangles;
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  // A multi-operation triangle batch must observe earlier operations while the
  // underlying graph still advances by exactly one logical version.
  {
    DynamicGraph graph(3, false);
    graph.bulk_load_edges({{0, 1}, {1, 2}});
    IncrementalTriangleCount triangles(graph);
    const auto before_version = graph.version();
    UpdateBatch batch;
    batch.add(0, 2);
    batch.remove(1, 2);
    triangles.apply(batch);
    assert(triangles.value() == 0);
    assert(graph.version() == before_version + 1);
    assert(graph.has_edge(0, 2));
    assert(!graph.has_edge(1, 2));
  }

  // Weighted SSSP regression: 10 -> 5 -> 8 in one batch must use the final
  // graph weight 8, never the obsolete intermediate weight 5.
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

  // Weighted storage follows the simple-graph contract and ignores self-loops.
  {
    WeightedDynamicGraph graph(2, false);
    WeightedUpdateBatch batch;
    batch.add(0, 0, 1);
    graph.apply(batch);
    assert(!graph.weight(0, 0).has_value());
  }

  // PageRank with a pre-existing dangling vertex must take the exact global
  // fallback even when the structural update is elsewhere in the graph.
  {
    DynamicGraph graph(5, true);
    graph.bulk_load_edges({{0, 1}, {1, 0}, {2, 3}, {3, 2}});  // vertex 4 dangling
    IncrementalPageRank pagerank(graph);
    UpdateBatch batch;
    batch.add(0, 2);
    pagerank.apply(batch);
    assert(pagerank.last_repaired_vertices() == graph.vertex_count());
    const auto validation = pagerank.validate_against_full();
    assert(validation.within_tolerance);
  }

  // Growing an undirected graph changes n and therefore PageRank teleportation
  // globally; this must also force a full solve even without dangling vertices.
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

  // Static CSR construction follows the same simple-graph rule for loops.
  {
    CsrGraph graph({{0, 0}, {0, 1}}, false);
    assert(!graph.has_edge(0, 0));
    assert(graph.has_edge(0, 1));
  }

  return 0;
}
