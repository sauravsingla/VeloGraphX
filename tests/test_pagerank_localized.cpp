#include <cassert>
#include <cmath>

#include "velographx/incremental/pagerank.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

void assert_close(const std::vector<double>& a, const std::vector<double>& b,
                  double tol = 1e-6) {
  assert(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    assert(std::abs(a[i] - b[i]) < tol);
  }
}

}  // namespace

int main() {
  using namespace velographx;

  // Two disconnected directed regions. Updating the first should propagate
  // through multiple hops without requiring the second region to be repaired.
  DynamicGraph g(8, true);
  UpdateBatch initial;
  initial.add(0, 1);
  initial.add(1, 2);
  initial.add(2, 3);
  initial.add(3, 0);
  initial.add(4, 5);
  initial.add(5, 6);
  initial.add(6, 7);
  initial.add(7, 4);
  g.apply(initial);

  IncrementalPageRank incremental(g);
  UpdateBatch update;
  update.add(0, 2);
  incremental.apply(update, 64, 1e-12, 0.95);
  assert(incremental.last_repair_iterations() > 1);
  assert(incremental.last_repaired_vertices() >= 3);
  assert(incremental.last_repaired_vertices() < g.vertex_count());

  // Force the safety fallback and compare against an independent full result.
  DynamicGraph full_graph(8, true);
  full_graph.apply(initial);
  full_graph.apply(update);
  IncrementalPageRank full(full_graph);

  DynamicGraph fallback_graph(8, true);
  fallback_graph.apply(initial);
  IncrementalPageRank fallback(fallback_graph);
  fallback.apply(update, 64, 1e-12, 0.20);
  assert(fallback.last_repaired_vertices() == fallback_graph.vertex_count());
  assert_close(fallback.values(), full.values());

  return 0;
}
