#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/sssp.hpp"
#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/types.hpp"

namespace foreign_graph {

using velographx::EdgeWeight;
using velographx::UpdateBatch;
using velographx::VertexId;
using velographx::WeightedUpdateBatch;

// Deliberately exposes no VeloGraphX-named member API. All adaptation is by ADL.
struct Graph {
  std::vector<std::vector<VertexId>> out;
  std::uint64_t revision{0};
};

std::size_t vx_vertex_count(const Graph& g) { return g.out.size(); }
bool vx_is_directed(const Graph&) { return false; }
std::uint64_t vx_version(const Graph& g) { return g.revision; }

template <class Fn>
void vx_for_each_neighbor(const Graph& g, VertexId u, Fn&& fn) {
  if (u >= g.out.size()) return;
  for (auto v : g.out[u]) fn(v);
}

template <class Fn>
void vx_for_each_in_neighbor(const Graph& g, VertexId v, Fn&& fn) {
  vx_for_each_neighbor(g, v, std::forward<Fn>(fn));
}

bool vx_has_edge(const Graph& g, VertexId u, VertexId v) {
  if (u >= g.out.size()) return false;
  return std::binary_search(g.out[u].begin(), g.out[u].end(), v);
}

void vx_apply_updates(Graph& g, const UpdateBatch& batch) {
  auto ensure = [&](VertexId v) {
    if (v >= g.out.size()) g.out.resize(static_cast<std::size_t>(v) + 1);
  };
  auto change = [&](VertexId u, VertexId v, bool add) {
    ensure(std::max(u, v));
    auto& row = g.out[u];
    auto it = std::lower_bound(row.begin(), row.end(), v);
    if (add) {
      if (it == row.end() || *it != v) row.insert(it, v);
    } else if (it != row.end() && *it == v) {
      row.erase(it);
    }
  };
  for (const auto& e : batch.updates) {
    change(e.src, e.dst, e.add);
    if (e.src != e.dst) change(e.dst, e.src, e.add);
  }
  ++g.revision;
}

struct WeightedGraph {
  std::vector<std::vector<std::pair<VertexId, EdgeWeight>>> out;
  std::uint64_t revision{0};
};

std::size_t vx_vertex_count(const WeightedGraph& g) { return g.out.size(); }
bool vx_is_directed(const WeightedGraph&) { return false; }
std::uint64_t vx_version(const WeightedGraph& g) { return g.revision; }

template <class Fn>
void vx_for_each_weighted_neighbor(const WeightedGraph& g, VertexId u, Fn&& fn) {
  if (u >= g.out.size()) return;
  for (const auto& [v, w] : g.out[u]) fn(v, w);
}

std::optional<EdgeWeight> vx_edge_weight(const WeightedGraph& g, VertexId u, VertexId v) {
  if (u >= g.out.size()) return std::nullopt;
  for (const auto& [dst, weight] : g.out[u]) {
    if (dst == v) return weight;
  }
  return std::nullopt;
}

void vx_apply_updates(WeightedGraph& g, const WeightedUpdateBatch& batch) {
  auto ensure = [&](VertexId v) {
    if (v >= g.out.size()) g.out.resize(static_cast<std::size_t>(v) + 1);
  };
  auto change = [&](VertexId u, VertexId v, EdgeWeight weight, bool add) {
    ensure(std::max(u, v));
    auto& row = g.out[u];
    auto it = std::find_if(row.begin(), row.end(), [&](const auto& item) { return item.first == v; });
    if (add) {
      if (it == row.end()) row.push_back({v, weight});
      else it->second = weight;
    } else if (it != row.end()) {
      row.erase(it);
    }
  };
  for (const auto& e : batch.updates) {
    change(e.src, e.dst, e.weight, e.add);
    if (e.src != e.dst) change(e.dst, e.src, e.weight, e.add);
  }
  ++g.revision;
}

}  // namespace foreign_graph

int main() {
  using namespace velographx;
  foreign_graph::Graph graph{{{1}, {0, 2}, {1, 3}, {2}, {}}};

  static_assert(ReadableGraph<foreign_graph::Graph>);
  static_assert(MutableGraph<foreign_graph::Graph>);

  BasicIncrementalBFS<foreign_graph::Graph> bfs(graph, 0);
  assert((bfs.distances() == std::vector<std::uint32_t>{0, 1, 2, 3, BasicIncrementalBFS<foreign_graph::Graph>::unreachable}));

  BasicIncrementalSSSP<foreign_graph::Graph> sssp(graph, 0);
  assert(sssp.distances()[3] == 3);

  UpdateBatch add;
  add.add(0, 4);
  bfs.apply(add);
  sssp.apply(add);
  assert(bfs.distances()[4] == 1);
  assert(sssp.distances()[4] == 1);

  UpdateBatch remove;
  remove.remove(1, 2);
  bfs.apply(remove);
  sssp.apply(remove);
  assert(bfs.distances()[2] == BasicIncrementalBFS<foreign_graph::Graph>::unreachable);
  assert(sssp.distances()[2] == incremental_detail::kDijkstraInf);

  foreign_graph::WeightedGraph weighted{{
      {{1, 5}, {2, 20}},
      {{0, 5}, {2, 3}},
      {{0, 20}, {1, 3}},
  }};
  static_assert(ReadableGraph<foreign_graph::WeightedGraph>);
  static_assert(MutableGraph<foreign_graph::WeightedGraph>);
  BasicIncrementalWeightedSSSP<foreign_graph::WeightedGraph> weighted_sssp(weighted, 0);
  assert(weighted_sssp.distances()[2] == 8);

  WeightedUpdateBatch improve;
  improve.add(0, 2, 2);
  weighted_sssp.apply(improve);
  assert(weighted_sssp.distances()[2] == 2);

  WeightedUpdateBatch worsen;
  worsen.update(0, 2, 50);
  weighted_sssp.apply(worsen);
  assert(weighted_sssp.distances()[2] == 8);

  return 0;
}
