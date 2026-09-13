#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <random>
#include <utility>
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

namespace {

std::vector<std::uint64_t> reference_dijkstra(const velographx::WeightedDynamicGraph& graph,
                                              velographx::VertexId source) {
  using namespace velographx;
  constexpr auto inf = incremental_detail::kDijkstraInf;
  std::vector<std::uint64_t> dist(graph.vertex_count(), inf);
  if (source >= graph.vertex_count()) return dist;
  using Item = std::pair<std::uint64_t, VertexId>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
  dist[source] = 0;
  queue.push({0, source});
  while (!queue.empty()) {
    const auto [d, u] = queue.top();
    queue.pop();
    if (d != dist[u]) continue;
    for (const auto& [v, w] : graph.neighbors(u)) {
      if (w > inf - d) continue;
      const auto candidate = d + w;
      if (candidate < dist[v]) {
        dist[v] = candidate;
        queue.push({candidate, v});
      }
    }
  }
  return dist;
}

}  // namespace

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

  // Same-edge multi-update regression: final graph weight, not an obsolete
  // intermediate weight, controls the maintained shortest path.
  WeightedUpdateBatch repeated;
  repeated.update(0, 1, 3);
  repeated.update(0, 1, 7);
  sssp.apply(repeated);
  assert(graph.weight(0, 1).has_value() && *graph.weight(0, 1) == 7);
  assert(sssp.distances() == reference_dijkstra(graph, 0));

  // Random differential campaign with repeated edges and mixed batch sizes.
  WeightedDynamicGraph random_graph(12, true);
  IncrementalWeightedSSSP random_sssp(random_graph, 0);
  std::mt19937 rng(20260913U);
  std::uniform_int_distribution<std::uint32_t> vertex_dist(0, 11);
  std::uniform_int_distribution<std::uint32_t> weight_dist(0, 50);
  std::uniform_int_distribution<int> batch_size_dist(1, 6);
  std::bernoulli_distribution add_dist(0.72);

  for (std::size_t step = 0; step < 500; ++step) {
    WeightedUpdateBatch batch;
    const auto batch_size = batch_size_dist(rng);
    for (int i = 0; i < batch_size; ++i) {
      VertexId u = vertex_dist(rng);
      VertexId v = vertex_dist(rng);
      if (u == v) v = static_cast<VertexId>((v + 1) % 12);
      if (add_dist(rng)) batch.update(u, v, weight_dist(rng));
      else batch.remove(u, v);

      // Periodically force another operation on the same edge so last-write
      // semantics are exercised deliberately rather than by chance alone.
      if ((step + static_cast<std::size_t>(i)) % 11 == 0) {
        batch.update(u, v, weight_dist(rng));
      }
    }
    random_sssp.apply(batch);
    assert(random_sssp.distances() == reference_dijkstra(random_graph, 0));
  }

  return 0;
}
