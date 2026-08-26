#include "velographx/algorithms.hpp"
#include <iostream>

int main() {
  velographx::CsrGraph graph({{0,1},{1,2},{2,0},{2,3}}, false);
  const auto distance = velographx::bfs_distances(graph, 0);
  std::cout << "vertices=" << graph.vertex_count() << " triangles=" << velographx::triangle_count(graph) << "\n";
  for (std::size_t v = 0; v < distance.size(); ++v) std::cout << "d(0," << v << ")=" << distance[v] << "\n";
}
