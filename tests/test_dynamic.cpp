#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <vector>

#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/runtime/execution_plan.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

bool equals(std::vector<velographx::VertexId> actual,
            std::initializer_list<velographx::VertexId> expected) {
  return actual == std::vector<velographx::VertexId>(expected);
}

void dump(const char* label, const std::vector<velographx::VertexId>& row) {
  std::cerr << label << ':';
  for (auto v : row) std::cerr << ' ' << v;
  std::cerr << '\n';
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
  dump("before0", g.neighbors(0));
  dump("before1", g.neighbors(1));
  dump("before2", g.neighbors(2));
  assert(g.version() == 1);
  assert(g.has_edge(0, 1));
  assert(g.edge_count_directed() == 6);
  assert(g.delta_edge_count() == 6);

  IncrementalTriangleCount tc(g);
  dump("after0", g.neighbors(0));
  dump("after1", g.neighbors(1));
  dump("after2", g.neighbors(2));
  std::cerr << "triangles=" << tc.value() << " base=" << g.base_edge_count_directed()
            << " delta=" << g.delta_edge_count() << '\n';
  assert(tc.value() == 1);
  assert(g.is_compact());
  assert(g.base_edge_count_directed() == 6);
  assert(g.delta_edge_count() == 0);

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
  assert(!directed.is_compact());
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

  directed.add_edge(100000, 2);
  assert(directed.vertex_count() == 100001);
  assert(directed.has_edge(100000, 2));
  const auto incoming_after_growth = directed.in_neighbors(2);
  assert(std::binary_search(incoming_after_growth.begin(),
                            incoming_after_growth.end(), 100000));

  assert(directed.storage_bytes() > 0);
  return 0;
}
