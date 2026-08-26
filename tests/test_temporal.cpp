#include "velographx/storage/temporal_graph.hpp"

#include <cassert>

int main() {
  using namespace velographx;

  TemporalGraph graph(4, false);
  UpdateBatch first;
  first.add(0, 1, 10);
  first.add(1, 2, 20);
  graph.apply(first);

  UpdateBatch second;
  second.add(2, 3, 30);
  second.remove(0, 1, 40);
  graph.apply(second);

  assert(graph.version() == 2);
  assert(graph.history().size() == 2);

  const auto v1 = graph.snapshot_version(1);
  assert(v1.has_edge(0, 1));
  assert(v1.has_edge(1, 2));
  assert(!v1.has_edge(2, 3));

  const auto t25 = graph.snapshot_time(25);
  assert(t25.has_edge(0, 1));
  assert(t25.has_edge(1, 2));
  assert(!t25.has_edge(2, 3));

  const auto t45 = graph.snapshot_time(45);
  assert(!t45.has_edge(0, 1));
  assert(t45.has_edge(2, 3));

  const auto version_changes = graph.changes_between_versions(1, 2);
  assert(version_changes.size() == 2);

  const auto time_changes = graph.changes_between_times(15, 35);
  assert(time_changes.size() == 2);

  const auto window = graph.sliding_window(35, 20);
  assert(window.has_edge(1, 2));
  assert(window.has_edge(2, 3));
  assert(!window.has_edge(0, 1));
  return 0;
}
