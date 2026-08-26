#include <cassert>

#include "velographx/incremental/connected_components.hpp"
#include "velographx/storage/dynamic_graph.hpp"

int main() {
  velographx::DynamicGraph g(8, false);
  velographx::UpdateBatch init;
  init.add(0, 1);
  init.add(1, 2);
  init.add(2, 3);
  init.add(4, 5);
  init.add(5, 6);
  g.apply(init);

  velographx::IncrementalComponents cc(g);
  assert(cc.component(0) == cc.component(3));
  assert(cc.component(4) == cc.component(6));
  assert(cc.component(0) != cc.component(4));

  velographx::UpdateBatch deletion;
  deletion.remove(1, 2);
  cc.apply(deletion);

  assert(cc.component(0) == cc.component(1));
  assert(cc.component(2) == cc.component(3));
  assert(cc.component(0) != cc.component(2));
  assert(cc.component(4) == cc.component(6));
  assert(cc.last_repaired_vertices() == 4);
  assert(cc.last_repaired_vertices() < g.vertex_count());

  velographx::UpdateBatch mixed;
  mixed.remove(4, 5);
  mixed.add(1, 2);
  cc.apply(mixed);

  assert(cc.component(0) == cc.component(3));
  assert(cc.component(4) != cc.component(5));
  assert(cc.component(5) == cc.component(6));
  assert(cc.last_repaired_vertices() == 3);

  return 0;
}
