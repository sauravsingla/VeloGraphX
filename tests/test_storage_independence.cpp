#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/pagerank.hpp"
#include "velographx/incremental/sssp.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

using namespace velographx;

int main() {
  const std::vector<std::pair<VertexId, VertexId>> edges = {
      {0, 1}, {1, 2}, {2, 0}, {2, 3}, {3, 4}, {4, 5}};

  DynamicGraph dynamic(6, false);
  dynamic.bulk_load_edges(edges);
  CsrGraph csr(edges, false);

  BasicIncrementalBFS<DynamicGraph> dynamic_bfs(dynamic, 0);
  BasicIncrementalBFS<CsrGraph> csr_bfs(csr, 0);
  assert(dynamic_bfs.distances() == csr_bfs.distances());

  BasicIncrementalSSSP<DynamicGraph> dynamic_sssp(dynamic, 0);
  BasicIncrementalSSSP<CsrGraph> csr_sssp(csr, 0);
  assert(dynamic_sssp.distances() == csr_sssp.distances());

  BasicIncrementalComponents<DynamicGraph> dynamic_components(dynamic);
  BasicIncrementalComponents<CsrGraph> csr_components(csr);
  for (VertexId v = 0; v < 6; ++v) {
    assert((dynamic_components.component(v) == dynamic_components.component(0)) ==
           (csr_components.component(v) == csr_components.component(0)));
  }

  BasicIncrementalKCore<DynamicGraph> dynamic_kcore(dynamic);
  BasicIncrementalKCore<CsrGraph> csr_kcore(csr);
  assert(dynamic_kcore.core() == csr_kcore.core());

  BasicIncrementalTriangleCount<DynamicGraph> dynamic_triangles(dynamic);
  BasicIncrementalTriangleCount<CsrGraph> csr_triangles(csr);
  assert(dynamic_triangles.value() == 1);
  assert(dynamic_triangles.value() == csr_triangles.value());

  BasicIncrementalPageRank<DynamicGraph> dynamic_pr(dynamic);
  BasicIncrementalPageRank<CsrGraph> csr_pr(csr);
  assert(dynamic_pr.values().size() == csr_pr.values().size());
  double l1 = 0.0;
  for (std::size_t i = 0; i < dynamic_pr.values().size(); ++i) {
    l1 += std::abs(dynamic_pr.values()[i] - csr_pr.values()[i]);
  }
  assert(l1 < 1e-10);

  // Compatibility aliases remain usable for the default dynamic backend.
  IncrementalBFS alias_bfs(dynamic, 0);
  assert(alias_bfs.distances() == dynamic_bfs.distances());

  return 0;
}
