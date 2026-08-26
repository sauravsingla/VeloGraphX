#include <cassert>

#include "velographx/incremental/weighted_sssp.hpp"

int main() {
  using namespace velographx;

  WeightedDynamicGraph graph(4, true);
  WeightedUpdateBatch initial;
  initial.add(0, 1, 5);
  initial.add(1, 2, 4);
  initial.add(0, 2, 20);
  initial.add(2, 3, 3);
  graph.apply(initial);

  IncrementalWeightedSSSP sssp(graph, 0);
  assert(sssp.distances()[0] == 0);
  assert(sssp.distances()[1] == 5);
  assert(sssp.distances()[2] == 9);
  assert(sssp.distances()[3] == 12);

  WeightedUpdateBatch decrease;
  decrease.update(0, 2, 2);
  sssp.apply(decrease);
  assert(sssp.distances()[2] == 2);
  assert(sssp.distances()[3] == 5);

  WeightedUpdateBatch insertion;
  insertion.add(1, 3, 1);
  sssp.apply(insertion);
  assert(sssp.distances()[3] == 5);

  WeightedUpdateBatch increase;
  increase.update(0, 2, 30);
  sssp.apply(increase);
  assert(sssp.distances()[2] == 9);
  assert(sssp.distances()[3] == 6);

  WeightedUpdateBatch deletion;
  deletion.remove(1, 3);
  sssp.apply(deletion);
  assert(sssp.distances()[3] == 12);

  return 0;
}
