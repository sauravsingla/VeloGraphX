#include "velographx/incremental/kcore.hpp"
#include "velographx/storage/dynamic_graph.hpp"

#include <cassert>
#include <cstddef>

int main() {
  using namespace velographx;

  DynamicGraph graph(6, false);
  UpdateBatch seed;
  seed.add(0, 1);
  seed.add(1, 2);
  seed.add(2, 0);
  seed.add(3, 4);
  seed.add(4, 5);
  seed.add(5, 3);
  graph.apply(seed);

  IncrementalKCore incremental(graph);
  assert(incremental.core()[0] == 2);
  assert(incremental.core()[3] == 2);

  // Removing one edge from the first triangle changes only that connected
  // component. The unrelated triangle must not be part of the repair region.
  UpdateBatch deletion;
  deletion.remove(0, 1);
  incremental.apply(deletion);
  assert(incremental.last_repaired_vertices() == 3);

  IncrementalKCore deletion_baseline(graph);
  assert(incremental.core() == deletion_baseline.core());
  assert(incremental.core()[0] == 1);
  assert(incremental.core()[1] == 1);
  assert(incremental.core()[2] == 1);
  assert(incremental.core()[3] == 2);
  assert(incremental.core()[4] == 2);
  assert(incremental.core()[5] == 2);

  // Inserting a bridge merges the two components, so the bounded repair must
  // expand to the newly merged six-vertex component and still match a full
  // recomputation exactly.
  UpdateBatch insertion;
  insertion.add(2, 3);
  incremental.apply(insertion);
  assert(incremental.last_repaired_vertices() == 6);

  IncrementalKCore insertion_baseline(graph);
  assert(incremental.core() == insertion_baseline.core());

  return 0;
}
