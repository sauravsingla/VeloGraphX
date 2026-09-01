#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "teseo.hpp"
#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace teseo_adapter {

using velographx::VertexId;

class Graph {
 public:
  Graph(std::size_t vertices, const std::vector<std::pair<VertexId, VertexId>>& edges)
      : vertices_(vertices) {
    auto tx = database_.start_transaction();
    for (std::size_t v = 0; v < vertices_; ++v) tx.insert_vertex(v);
    for (const auto& [u, v] : edges) tx.insert_edge(u, v, 1.0);
    tx.commit();
  }

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  ~Graph() {
    if (iterator_) iterator_->close();
    if (snapshot_) snapshot_->commit();
  }

  void ensure_snapshot() const {
    if (snapshot_) return;
    snapshot_ = std::make_unique<teseo::Transaction>(database_.start_transaction(true));
    iterator_ = std::make_unique<teseo::Iterator>(snapshot_->iterator());
  }

  mutable teseo::Teseo database_;
  std::size_t vertices_{0};
  mutable std::unique_ptr<teseo::Transaction> snapshot_;
  mutable std::unique_ptr<teseo::Iterator> iterator_;
};

// Non-intrusive customization only: Graph intentionally has no vertex_count(),
// directed(), neighbors(), or for_each_neighbor() member functions.
std::size_t vx_vertex_count(const Graph& graph) { return graph.vertices_; }
bool vx_is_directed(const Graph&) { return false; }

template <class Fn>
void vx_for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  graph.ensure_snapshot();
  graph.iterator_->edges(static_cast<std::uint64_t>(u), false, [&](std::uint64_t destination) {
    fn(static_cast<VertexId>(destination));
  });
}

}  // namespace teseo_adapter

namespace {

using velographx::BasicIncrementalBFS;
using velographx::CsrGraph;
using velographx::DynamicGraph;
using velographx::VertexId;

std::vector<std::pair<VertexId, VertexId>> make_graph(std::size_t vertices) {
  std::set<std::pair<VertexId, VertexId>> unique;
  constexpr VertexId offsets[] = {1, 7, 31, 127};
  for (VertexId u = 0; u < vertices; ++u) {
    for (auto offset : offsets) {
      VertexId v = static_cast<VertexId>((static_cast<std::size_t>(u) + offset) % vertices);
      if (u == v) continue;
      auto edge = std::minmax(u, v);
      unique.insert({edge.first, edge.second});
    }
  }
  return {unique.begin(), unique.end()};
}

template <class Graph>
double median_recompute_us(Graph& graph, VertexId source, std::vector<std::uint32_t>& distances) {
  BasicIncrementalBFS<Graph> bfs(graph, source);
  std::vector<double> samples;
  samples.reserve(5);
  for (int rep = 0; rep < 5; ++rep) {
    const auto start = std::chrono::steady_clock::now();
    bfs.recompute();
    const auto stop = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
  }
  std::sort(samples.begin(), samples.end());
  distances = bfs.distances();
  return samples[samples.size() / 2];
}

std::uint64_t digest(const std::vector<std::uint32_t>& distances) {
  std::uint64_t h = 1469598103934665603ULL;
  for (auto d : distances) {
    h ^= static_cast<std::uint64_t>(d);
    h *= 1099511628211ULL;
  }
  return h;
}

}  // namespace

int main(int argc, char** argv) {
  const std::size_t vertices = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 8192;
  const VertexId source = 0;
  const auto edges = make_graph(vertices);

  DynamicGraph dynamic(vertices, false);
  dynamic.bulk_load_edges(edges);
  CsrGraph csr(edges, false);
  teseo_adapter::Graph teseo_graph(vertices, edges);

  std::vector<std::uint32_t> dynamic_dist;
  std::vector<std::uint32_t> csr_dist;
  std::vector<std::uint32_t> teseo_dist;

  const double dynamic_us = median_recompute_us(dynamic, source, dynamic_dist);
  const double csr_us = median_recompute_us(csr, source, csr_dist);
  const double teseo_us = median_recompute_us(teseo_graph, source, teseo_dist);

  const bool exact = dynamic_dist == csr_dist && dynamic_dist == teseo_dist;
  std::cout << std::fixed << std::setprecision(3)
            << "{\"schema_version\":1,"
            << "\"benchmark\":\"same-algorithm-storage-bfs\","
            << "\"algorithm\":\"BasicIncrementalBFS::recompute\","
            << "\"vertices\":" << vertices << ','
            << "\"edges\":" << edges.size() << ','
            << "\"source\":" << source << ','
            << "\"repetitions\":5,"
            << "\"exact\":" << (exact ? "true" : "false") << ','
            << "\"digest\":" << digest(dynamic_dist) << ','
            << "\"dynamic_graph_median_us\":" << dynamic_us << ','
            << "\"csr_graph_median_us\":" << csr_us << ','
            << "\"teseo_median_us\":" << teseo_us << ','
            << "\"dynamic_over_csr\":" << (dynamic_us / csr_us) << ','
            << "\"dynamic_over_teseo\":" << (dynamic_us / teseo_us)
            << "}\n";

  return exact ? 0 : 2;
}
