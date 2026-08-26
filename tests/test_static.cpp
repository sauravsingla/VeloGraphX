#include "velographx/algorithms.hpp"
#include "velographx/csr_graph.hpp"
#include <cassert>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace velographx;

int main() {
  CsrGraph g({{0,1},{1,2},{2,0},{2,3}}, false);
  assert(g.vertex_count() == 4);
  assert(g.edge_entry_count() == 8);
  assert(g.degree(2) == 3);

  const auto d = bfs_distances(g, 0);
  assert(d[0] == 0 && d[1] == 1 && d[2] == 1 && d[3] == 2);

  const auto cc = connected_components(g);
  assert(cc[0] == cc[3]);

  assert(common_neighbor_count(g, 0, 1) == 1);
  assert(std::abs(jaccard_similarity(g, 0, 1) - (1.0/3.0)) < 1e-12);
  assert(triangle_count(g) == 1);

  const auto pr = pagerank(g);
  assert(pr.size() == 4);
  const double sum = std::accumulate(pr.begin(), pr.end(), 0.0);
  assert(std::abs(sum - 1.0) < 1e-9);

  CsrGraph separate({{0,1},{2,3}}, false);
  const auto components = connected_components(separate);
  assert(components[0] == components[1]);
  assert(components[2] == components[3]);
  assert(components[0] != components[2]);

  bool threw = false;
  try { (void)bfs_distances(g, 99); } catch (const std::out_of_range&) { threw = true; }
  assert(threw);
  return 0;
}
