#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
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

  ~Graph() { close_snapshot(); }

  void close_snapshot() const {
    if (iterator_) {
      iterator_->close();
      iterator_.reset();
    }
    if (snapshot_) {
      snapshot_->commit();
      snapshot_.reset();
    }
  }

  void ensure_snapshot() const {
    if (snapshot_) return;
    snapshot_ = std::make_unique<teseo::Transaction>(database_.start_transaction(true));
    iterator_ = std::make_unique<teseo::Iterator>(snapshot_->iterator());
  }

  void apply(const UpdateBatch& batch) {
    close_snapshot();
    auto tx = database_.start_transaction();
    for (const auto& op : batch.updates) {
      if (op.add) tx.insert_edge(op.src, op.dst, 1.0);
      else tx.remove_edge(op.src, op.dst);
    }
    tx.commit();
    if (!batch.empty()) ++version_;
  }

  mutable teseo::Teseo database_;
  std::size_t vertices_{0};
  std::uint64_t version_{0};
  mutable std::unique_ptr<teseo::Transaction> snapshot_;
  mutable std::unique_ptr<teseo::Iterator> iterator_;
};

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
  return graph.snapshot_->has_edge(u, v);
}

void vx_apply_updates(Graph& graph, const UpdateBatch& batch) { graph.apply(batch); }

}  // namespace teseo_adapter

namespace {

using velographx::BasicIncrementalBFS;
using velographx::CsrGraph;
using velographx::DynamicGraph;
using velographx::UpdateBatch;
using velographx::VertexId;

struct InputGraph {
  std::size_t vertices{0};
  std::vector<std::pair<VertexId, VertexId>> edges;
};

InputGraph make_synthetic(std::size_t vertices) {
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
  return {vertices, {unique.begin(), unique.end()}};
}

InputGraph load_undirected_edge_list(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list: " + path);
  std::set<std::pair<VertexId, VertexId>> unique;
  std::size_t vertices = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '%') continue;
    std::istringstream iss(line);
    std::uint64_t a = 0, b = 0;
    if (!(iss >> a >> b)) continue;
    if (a == b) continue;
    const auto u = static_cast<VertexId>(a);
    const auto v = static_cast<VertexId>(b);
    const auto edge = std::minmax(u, v);
    unique.insert({edge.first, edge.second});
    vertices = std::max(vertices, static_cast<std::size_t>(std::max(u, v)) + 1);
  }
  return {vertices, {unique.begin(), unique.end()}};
}

template <class Graph>
double median_recompute_us(Graph& graph, VertexId source, std::vector<std::uint32_t>& distances) {
  BasicIncrementalBFS<Graph> bfs(graph, source);
  std::vector<double> samples;
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

UpdateBatch make_update_batch(std::size_t vertices,
                              const std::vector<std::pair<VertexId, VertexId>>& edges,
                              std::size_t count) {
  UpdateBatch batch;
  std::set<std::pair<VertexId, VertexId>> present(edges.begin(), edges.end());
  const std::size_t removals = std::min(count / 2, edges.size());
  for (std::size_t i = 0; i < removals; ++i) batch.remove(edges[i].first, edges[i].second);
  std::size_t added = 0;
  for (VertexId u = 0; u < vertices && added < count - removals; ++u) {
    VertexId v = static_cast<VertexId>((static_cast<std::size_t>(u) * 97 + 53) % vertices);
    if (u == v) continue;
    auto e = std::minmax(u, v);
    if (!present.contains({e.first, e.second})) {
      batch.add(e.first, e.second);
      present.insert({e.first, e.second});
      ++added;
    }
  }
  return batch;
}

template <class Graph>
double apply_batch_us(Graph& graph, VertexId source, const UpdateBatch& batch,
                      std::vector<std::uint32_t>& distances) {
  BasicIncrementalBFS<Graph> bfs(graph, source);
  const auto start = std::chrono::steady_clock::now();
  bfs.apply(batch);
  const auto stop = std::chrono::steady_clock::now();
  distances = bfs.distances();
  return std::chrono::duration<double, std::micro>(stop - start).count();
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
  InputGraph input;
  std::string dataset = "synthetic";
  if (argc > 2 && std::string(argv[1]) == "--edge-list") {
    dataset = argv[2];
    input = load_undirected_edge_list(argv[2]);
  } else {
    const std::size_t vertices = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 8192;
    input = make_synthetic(vertices);
  }
  if (input.vertices == 0) return 3;
  const VertexId source = 0;

  DynamicGraph dynamic(input.vertices, false);
  dynamic.bulk_load_edges(input.edges);
  CsrGraph csr(input.edges, false);
  teseo_adapter::Graph teseo_graph(input.vertices, input.edges);

  std::vector<std::uint32_t> dynamic_dist, csr_dist, teseo_dist;
  const double dynamic_us = median_recompute_us(dynamic, source, dynamic_dist);
  const double csr_us = median_recompute_us(csr, source, csr_dist);
  const double teseo_us = median_recompute_us(teseo_graph, source, teseo_dist);
  const bool recompute_exact = dynamic_dist == csr_dist && dynamic_dist == teseo_dist;

  DynamicGraph dynamic_mut(input.vertices, false);
  dynamic_mut.bulk_load_edges(input.edges);
  teseo_adapter::Graph teseo_mut(input.vertices, input.edges);
  const auto batch = make_update_batch(input.vertices, input.edges, 64);
  std::vector<std::uint32_t> dynamic_after, teseo_after;
  const double dynamic_update_us = apply_batch_us(dynamic_mut, source, batch, dynamic_after);
  const double teseo_update_us = apply_batch_us(teseo_mut, source, batch, teseo_after);
  const bool update_exact = dynamic_after == teseo_after;

  const bool exact = recompute_exact && update_exact;
  std::cout << std::fixed << std::setprecision(3)
            << "{\"schema_version\":2,"
            << "\"benchmark\":\"same-algorithm-storage-bfs\","
            << "\"algorithm\":\"BasicIncrementalBFS\","
            << "\"dataset\":\"" << dataset << "\","
            << "\"vertices\":" << input.vertices << ','
            << "\"edges\":" << input.edges.size() << ','
            << "\"source\":" << source << ','
            << "\"repetitions\":5,"
            << "\"update_count\":" << batch.updates.size() << ','
            << "\"recompute_exact\":" << (recompute_exact ? "true" : "false") << ','
            << "\"update_exact\":" << (update_exact ? "true" : "false") << ','
            << "\"exact\":" << (exact ? "true" : "false") << ','
            << "\"digest\":" << digest(dynamic_dist) << ','
            << "\"updated_digest\":" << digest(dynamic_after) << ','
            << "\"dynamic_graph_median_us\":" << dynamic_us << ','
            << "\"csr_graph_median_us\":" << csr_us << ','
            << "\"teseo_median_us\":" << teseo_us << ','
            << "\"dynamic_update_us\":" << dynamic_update_us << ','
            << "\"teseo_update_us\":" << teseo_update_us
            << "}\n";

  return exact ? 0 : 2;
}
