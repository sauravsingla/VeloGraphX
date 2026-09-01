#include <cassert>
#include <map>
#include <optional>
#include <vector>

#include "velographx/incremental/weighted_sssp.hpp"

namespace foreign_weighted {

using velographx::EdgeWeight;
using velographx::VertexId;
using velographx::WeightedUpdateBatch;

struct Graph {
  explicit Graph(std::size_t vertices, bool directed = true)
      : directed(directed), adjacency(vertices) {}

  bool directed{true};
  std::vector<std::map<VertexId, EdgeWeight>> adjacency;
  std::uint64_t version{0};
};

std::size_t vx_vertex_count(const Graph& graph) { return graph.adjacency.size(); }
bool vx_is_directed(const Graph& graph) { return graph.directed; }
std::uint64_t vx_version(const Graph& graph) { return graph.version; }

template <class Fn>
void vx_for_each_weighted_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  if (u >= graph.adjacency.size()) return;
  for (const auto& [v, w] : graph.adjacency[u]) fn(v, w);
}

std::optional<EdgeWeight> vx_edge_weight(const Graph& graph, VertexId u, VertexId v) {
  if (u >= graph.adjacency.size()) return std::nullopt;
  const auto it = graph.adjacency[u].find(v);
  if (it == graph.adjacency[u].end()) return std::nullopt;
  return it->second;
}

void vx_apply_updates(Graph& graph, const WeightedUpdateBatch& batch) {
  for (const auto& op : batch.updates) {
    const auto n = static_cast<std::size_t>(std::max(op.src, op.dst)) + 1;
    if (n > graph.adjacency.size()) graph.adjacency.resize(n);
    auto apply_one = [&](VertexId u, VertexId v) {
      if (op.add) graph.adjacency[u][v] = op.weight;
      else graph.adjacency[u].erase(v);
    };
    apply_one(op.src, op.dst);
    if (!graph.directed && op.src != op.dst) apply_one(op.dst, op.src);
  }
  if (!batch.empty()) ++graph.version;
}

}  // namespace foreign_weighted

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

  foreign_weighted::Graph foreign(4, true);
  foreign_weighted::vx_apply_updates(foreign, initial);
  BasicIncrementalWeightedSSSP<foreign_weighted::Graph> foreign_sssp(foreign, 0);
  assert(foreign_sssp.distances() == sssp.distances());

  WeightedUpdateBatch decrease;
  decrease.update(0, 2, 2);
  sssp.apply(decrease);
  foreign_sssp.apply(decrease);
  assert(sssp.distances()[2] == 2);
  assert(sssp.distances()[3] == 5);
  assert(foreign_sssp.distances() == sssp.distances());

  WeightedUpdateBatch insertion;
  insertion.add(1, 3, 1);
  sssp.apply(insertion);
  foreign_sssp.apply(insertion);
  assert(sssp.distances()[3] == 5);
  assert(foreign_sssp.distances() == sssp.distances());

  WeightedUpdateBatch increase;
  increase.update(0, 2, 30);
  sssp.apply(increase);
  foreign_sssp.apply(increase);
  assert(sssp.distances()[2] == 9);
  assert(sssp.distances()[3] == 6);
  assert(foreign_sssp.distances() == sssp.distances());

  WeightedUpdateBatch deletion;
  deletion.remove(1, 3);
  sssp.apply(deletion);
  foreign_sssp.apply(deletion);
  assert(sssp.distances()[3] == 12);
  assert(foreign_sssp.distances() == sssp.distances());

  return 0;
}
