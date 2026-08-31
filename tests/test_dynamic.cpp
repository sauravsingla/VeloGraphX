#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <utility>
#include <vector>

#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/runtime/execution_plan.hpp"
#include "velographx/storage/consolidation.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

bool equals(std::vector<velographx::VertexId> actual,
            std::initializer_list<velographx::VertexId> expected) {
  return actual == std::vector<velographx::VertexId>(expected);
}

}  // namespace

int main() {
  using namespace velographx;

  DynamicGraph g(4, false);
  UpdateBatch b;
  b.add(0, 1);
  b.add(1, 2);
  b.add(2, 0);
  g.apply(b);
  assert(g.version() == 1);
  assert(g.has_edge(0, 1));
  assert(g.edge_count_directed() == 6);
  assert(!g.is_compact());
  assert(g.delta_edge_count() == 6);

  // Constructing an algorithm must observe the logical graph without changing
  // the underlying storage representation. This is part of the storage-
  // independent algorithm contract: callers, not algorithms, own compaction.
  const auto base_before_triangle = g.base_edge_count_directed();
  const auto delta_before_triangle = g.delta_edge_count();
  const auto version_before_triangle = g.version();
  IncrementalTriangleCount tc(g);
  assert(tc.value() == 1);
  assert(!g.is_compact());
  assert(g.base_edge_count_directed() == base_before_triangle);
  assert(g.delta_edge_count() == delta_before_triangle);
  assert(g.version() == version_before_triangle);

  UpdateBatch b2;
  b2.add(2, 3);
  tc.apply(b2);
  assert(tc.value() == 1);
  IncrementalComponents cc(g);
  assert(cc.component(0) == cc.component(3));

  auto plan = choose_execution({1, 1000, 4, 100, 1.2});
  assert(plan.mode == ExecutionMode::incremental);

  DynamicGraph directed(6, true);
  directed.bulk_load_edges({{0, 2}, {0, 1}, {0, 2}, {3, 2}, {4, 2}, {2, 5}});
  assert(directed.is_compact());
  assert(directed.edge_count_directed() == 5);
  assert(equals(directed.neighbors(0), {1, 2}));
  assert(equals(directed.in_neighbors(2), {0, 3, 4}));
  assert(equals(directed.in_neighbors(5), {2}));

  const auto compact_out = directed.compact_neighbors(0);
  assert(compact_out.size() == 2 && compact_out[0] == 1 && compact_out[1] == 2);
  const auto compact_in = directed.compact_in_neighbors(2);
  assert(compact_in.size() == 3 && compact_in[0] == 0 && compact_in[2] == 4);

  UpdateBatch delta;
  delta.remove(0, 2);
  delta.add(1, 2);
  delta.add(5, 2);
  directed.apply(delta);
  assert(!directed.has_edge(0, 2));
  assert(directed.has_edge(1, 2));
  assert(directed.has_edge(5, 2));
  assert(equals(directed.neighbors(0), {1}));
  assert(equals(directed.in_neighbors(2), {1, 3, 4, 5}));
  assert(directed.edge_count_directed() == 6);

  directed.add_edge(0, 2);
  assert(directed.has_edge(0, 2));
  assert(equals(directed.in_neighbors(2), {0, 1, 3, 4, 5}));

  const auto before_compact_out = directed.neighbors(2);
  const auto before_compact_in = directed.in_neighbors(2);
  const auto before_count = directed.edge_count_directed();
  directed.compact();
  assert(directed.is_compact());
  assert(directed.delta_edge_count() == 0);
  assert(directed.edge_count_directed() == before_count);
  assert(directed.neighbors(2) == before_compact_out);
  assert(directed.in_neighbors(2) == before_compact_in);

  // A row that has already been compacted into a sparse patch must remain
  // fully mutable on later batches, including reverse-adjacency maintenance.
  directed.remove_edge(0, 2);
  directed.add_edge(0, 4);
  assert(!directed.has_edge(0, 2));
  assert(directed.has_edge(0, 4));
  const auto in_two_after_repatch = directed.in_neighbors(2);
  assert(!std::binary_search(in_two_after_repatch.begin(), in_two_after_repatch.end(), 0));
  const auto in_four = directed.in_neighbors(4);
  assert(std::binary_search(in_four.begin(), in_four.end(), 0));
  const auto repatch_count = directed.edge_count_directed();
  directed.compact();
  assert(directed.is_compact());
  assert(directed.edge_count_directed() == repatch_count);
  assert(!directed.has_edge(0, 2));
  assert(directed.has_edge(0, 4));

  // Consolidation builds a fresh canonical CSR snapshot without mutating the
  // source graph. The forward and reverse logical views must be preserved.
  const auto source_storage = directed.storage_bytes();
  const auto source_count = directed.edge_count_directed();
  const auto source_neighbors_zero = directed.neighbors(0);
  const auto source_in_four = directed.in_neighbors(4);
  auto consolidated = consolidate_to_csr_snapshot(directed);
  assert(consolidated.directed_edges == source_count);
  assert(consolidated.source_storage_bytes == source_storage);
  assert(consolidated.consolidated_storage_bytes > 0);
  assert(consolidated.graph.is_compact());
  assert(consolidated.graph.edge_count_directed() == source_count);
  assert(consolidated.graph.neighbors(0) == source_neighbors_zero);
  assert(consolidated.graph.in_neighbors(4) == source_in_four);
  assert(directed.neighbors(0) == source_neighbors_zero);

  const auto below = evaluate_consolidation(120, 100, 120.0, 100.0);
  assert(!below.should_consolidate);
  const auto storage_signal = evaluate_consolidation(125, 100, 100.0, 100.0);
  assert(storage_signal.storage_limit_exceeded);
  assert(storage_signal.should_consolidate);
  const auto latency_signal = evaluate_consolidation(110, 100, 125.0, 100.0);
  assert(latency_signal.latency_limit_exceeded);
  assert(latency_signal.should_consolidate);

  directed.add_edge(100000, 2);
  assert(directed.vertex_count() == 100001);
  assert(directed.has_edge(100000, 2));
  const auto incoming_after_growth = directed.in_neighbors(2);
  assert(std::binary_search(incoming_after_growth.begin(),
                            incoming_after_growth.end(), 100000));

  // Exercise sparse row tracking below the automatic global-delta threshold.
  std::vector<std::pair<VertexId, VertexId>> seed;
  seed.reserve(400);
  for (VertexId i = 0; i < 200; ++i) {
    seed.emplace_back(i, 1000 + i);
    seed.emplace_back(65536 + i, 67000 + i);
  }
  DynamicGraph segmented(131072, true);
  segmented.bulk_load_edges(seed);
  assert(segmented.dirty_out_segment_count() == 0);
  assert(segmented.dirty_in_segment_count() == 0);

  UpdateBatch sparse;
  sparse.add(10, 50000);
  segmented.apply(sparse);
  assert(!segmented.is_compact());
  assert(segmented.dirty_out_segment_count() == 1);
  assert(segmented.dirty_in_segment_count() == 1);
  assert(segmented.has_edge(10, 50000));
  const auto reverse_first = segmented.in_neighbors(50000);
  assert(std::binary_search(reverse_first.begin(), reverse_first.end(), 10));

  sparse = {};
  sparse.add(65540, 120000);
  segmented.apply(sparse);
  assert(segmented.dirty_out_segment_count() == 2);
  assert(segmented.dirty_in_segment_count() == 2);

  const auto segmented_count = segmented.edge_count_directed();
  segmented.compact();
  assert(segmented.is_compact());
  assert(segmented.dirty_out_segment_count() == 0);
  assert(segmented.dirty_in_segment_count() == 0);
  assert(segmented.edge_count_directed() == segmented_count);
  assert(segmented.has_edge(10, 50000));
  assert(segmented.has_edge(65540, 120000));

  assert(directed.storage_bytes() > 0);
  return 0;
}
