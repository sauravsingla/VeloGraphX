#include <iostream>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/runtime/execution_plan.hpp"
int main(){
  velographx::DynamicGraph g(6,false); velographx::UpdateBatch initial; initial.add(0,1); initial.add(1,2); initial.add(2,0); g.apply(initial); g.compact();
  velographx::IncrementalTriangleCount tri(g); velographx::UpdateBatch update; update.add(2,3); update.add(3,4); tri.apply(update);
  auto plan=velographx::choose_execution({update.updates.size(),g.edge_count_directed(),4,g.vertex_count(),1.2});
  std::cout<<"triangles="<<tri.value()<<" "<<velographx::explain(plan)<<"\n";
}
