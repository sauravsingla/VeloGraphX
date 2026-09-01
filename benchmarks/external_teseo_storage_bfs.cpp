#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "teseo.hpp"
#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace teseo_adapter {

using velographx::UpdateBatch;
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

  ~Graph() { reset_snapshot(); }

  void ensure_snapshot() const {
    if (snapshot_) return;
    snapshot_ = std::make_unique<teseo::Transaction>(database_.start_transaction(true));
    iterator_ = std::make_unique<teseo::Iterator>(snapshot_->iterator());
  }

  void reset_snapshot() const {
    if (iterator_) {
      iterator_->close();
      iterator_.reset();
    }
    if (snapshot_) {
      snapshot_->commit();
      snapshot_.reset();
    }
  }

  mutable teseo::Teseo database_;
  std::size_t vertices_{0};
  std::uint64_t version_{0};
  mutable std::unique_ptr<teseo::Transaction> snapshot_;
  mutable std::unique_ptr<teseo::Iterator> iterator_;
};

// Non-intrusive customization only: Graph intentionally has no VeloGraphX
// graph API members. Both read and mutation paths are supplied through ADL.
std::size_t vx_vertex_count(const Graph& graph) { return graph.vertices_; }
bool vx_is_directed(const Graph&) { return false; }
std::uint64_t vx_version(const Graph& graph) { return graph.version_; }

template <class Fn>
void vx_for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  graph.ensure_snapshot();
  graph.iterator_->edges(static_cast<std::uint64_t>(u), false, [&](std::uint64_t destination) {
    fn(static_cast<VertexId>(destination));
  });
}

bool vx_has_edge(const Graph& graph, VertexId u, VertexId v) {
  graph.ensure_snapshot();
  return graph.snapshot_->has_edge(static_cast<std::uint64_t>(u), static_cast<std::uint64_t>(v));
}

void vx_apply_updates(Graph& graph, const UpdateBatch& batch) {
  if (batch.empty()) return;
  graph.reset_snapshot();
  auto tx = graph.database_.start_transaction();
  for (const auto& op : batch.updates) {
    const auto u = static_cast<std::uint64_t>(op.src);
    const auto v = static_cast<std::uint64_t>(op.dst);
    if (op.add) {
      if (!tx.has_edge(u, v)) tx.insert_edge(u, v, 1.0);
    } else {
      if (tx.has_edge(u, v)) tx.remove_edge(u, v);
    }
  }
  tx.commit();
  ++graph.version_;
}

}  // namespace teseo_adapter

namespace {

using velographx::BasicIncrementalBFS;
using velographx::CsrGraph;
using velographx::DynamicGraph;
using velographx::UpdateBatch;
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

std::vector<UpdateBatch> make_incremental_batches(std::size_t vertices) {
  std::vector<UpdateBatch> batches;
  batches.reserve(5);
  for (VertexId i = 0; i < 5; ++i) {
    UpdateBatch batch;
    const VertexId u = static_cast<VertexId>((i * 17) % vertices);
    const VertexId ring = static_cast<VertexId>((static_cast<std::size_t>(u) + 1) % vertices);
    const VertexId chord = static_cast<VertexId>((static_cast<std::size_t>(u) + 3) % vertices);
    batch.remove(std::min(u, ring), std::max(u, ring), i * 2);
    batch.add(std::min(u, chord), std::max(u, chord), i * 2 + 1);
    batches.push_back(std::move(batch));
  }
  return batches;
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
  const bool recompute_exact = dynamic_dist == csr_dist && dynamic_dist == teseo_dist;

  BasicIncrementalBFS<DynamicGraph> dynamic_incremental(dynamic, source);
  BasicIncrementalBFS<teseo_adapter::Graph> teseo_incremental(teseo_graph, source);
  std::vector<double> dynamic_apply_samples;
  std::vector<double> teseo_apply_samples;
  bool incremental_exact = true;

  for (const auto& batch : make_incremental_batches(vertices)) {
    auto start = std::chrono::steady_clock::now();
    dynamic_incremental.apply(batch);
    auto stop = std::chrono::steady_clock::now();
    dynamic_apply_samples.push_back(std::chrono::duration<double, std::micro>(stop - start).count());

    start = std::chrono::steady_clock::now();
    teseo_incremental.apply(batch);
    stop = std::chrono::steady_clock::now();
    teseo_apply_samples.push_back(std::chrono::duration<double, std::micro>(stop - start).count());

    incremental_exact = incremental_exact &&
        dynamic_incremental.distances() == teseo_incremental.distances();
  }
  std::sort(dynamic_apply_samples.begin(), dynamic_apply_samples.end());
  std::sort(teseo_apply_samples.begin(), teseo_apply_samples.end());
  const double dynamic_apply_us = dynamic_apply_samples[dynamic_apply_samples.size() / 2];
  const double teseo_apply_us = teseo_apply_samples[teseo_apply_samples.size() / 2];

  const bool exact = recompute_exact && incremental_exact;
  std::cout << std::fixed << std::setprecision(3)
            << "{\"schema_version\":2,"
            << "\"benchmark\":\"same-algorithm-storage-bfs\","
            << "\"algorithm\":\"BasicIncrementalBFS\","
            << "\"vertices\":" << vertices << ','
            << "\"edges\":" << edges.size() << ','
            << "\"source\":" << source << ','
            << "\"repetitions\":5,"
            << "\"exact\":" << (exact ? "true" : "false") << ','
            << "\"recompute_exact\":" << (recompute_exact ? "true" : "false") << ','
            << "\"incremental_exact\":" << (incremental_exact ? "true" : "false") << ','
            << "\"incremental_batches\":5,"
            << "\"digest\":" << digest(dynamic_incremental.distances()) << ','
            << "\"dynamic_graph_median_us\":" << dynamic_us << ','
            << "\"csr_graph_median_us\":" << csr_us << ','
            << "\"teseo_median_us\":" << teseo_us << ','
            << "\"dynamic_incremental_median_us\":" << dynamic_apply_us << ','
            << "\"teseo_incremental_median_us\":" << teseo_apply_us << ','
            << "\"dynamic_over_csr\":" << (dynamic_us / csr_us) << ','
            << "\"dynamic_over_teseo\":" << (dynamic_us / teseo_us)
            << "}\n";

  return exact ? 0 : 2;
}
