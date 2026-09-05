#include <velographx/algorithms.hpp>

int main() {
  velographx::CsrGraph graph({{0, 1}, {1, 2}, {2, 0}}, false);
  const auto distances = velographx::bfs_distances(graph, 0);
  return distances.size() == graph.vertex_count() ? 0 : 1;
}
