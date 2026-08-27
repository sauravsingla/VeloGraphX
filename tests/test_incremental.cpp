#include <cassert>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/sssp.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/pagerank.hpp"
#include "velographx/incremental/triangles.hpp"

int main() {
  velographx::DynamicGraph g(5, false);
  velographx::UpdateBatch b;
  b.add(0, 1);
  b.add(1, 2);
  g.apply(b);

  velographx::IncrementalBFS bfs(g, 0);
  assert(bfs.distances()[2] == 2);
  velographx::UpdateBatch c;
  c.add(2, 3);
  bfs.apply(c);
  assert(bfs.distances()[3] == 3);
  velographx::IncrementalSSSP s(g, 0);
  assert(s.distances()[3] == 3);
  velographx::IncrementalKCore kc(g);
  assert(kc.core().size() == 5);
  velographx::IncrementalPageRank pr(g);
  assert(pr.values().size() == 5);

  // Regression: later additions in one batch must observe earlier additions.
  // Starting from 0-1, adding 1-2 and 0-2 together creates exactly one triangle.
  velographx::DynamicGraph tg(3, false);
  tg.add_edge(0, 1);
  velographx::IncrementalTriangleCount triangles(tg);
  const auto before_version = tg.version();
  velographx::UpdateBatch triangle_batch;
  triangle_batch.add(1, 2);
  triangle_batch.add(0, 2);
  triangles.apply(triangle_batch);
  assert(triangles.value() == 1);
  assert(tg.version() == before_version + 1);
  triangles.recompute();
  assert(triangles.value() == 1);
}
