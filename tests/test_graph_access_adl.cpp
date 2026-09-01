#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/sssp.hpp"
#include "velographx/types.hpp"

namespace foreign_graph {

using velographx::UpdateBatch;
using velographx::VertexId;

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

  return 0;
}
