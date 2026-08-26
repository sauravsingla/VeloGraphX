#include <cassert>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/incremental/connected_components.hpp"
#include "velographx/runtime/execution_plan.hpp"

int main() {
  velographx::DynamicGraph g(4, false);
  velographx::UpdateBatch b; b.add(0,1); b.add(1,2); b.add(2,0);
  g.apply(b);
  assert(g.version() == 1);
  assert(g.has_edge(0,1));
  velographx::IncrementalTriangleCount tc(g);
  assert(tc.value() == 1);
  velographx::UpdateBatch b2; b2.add(2,3); tc.apply(b2); assert(tc.value() == 1);
  velographx::IncrementalComponents cc(g); assert(cc.component(0) == cc.component(3));
  auto plan = velographx::choose_execution({1,1000,4,100,1.2});
  assert(plan.mode == velographx::ExecutionMode::incremental);
  return 0;
}
