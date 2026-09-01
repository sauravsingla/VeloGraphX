#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

#include <boost/graph/adjacency_list.hpp>

#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace boost_adapter {

using velographx::UpdateBatch;
using velographx::VertexId;
using Storage = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS>;

struct Graph {
  explicit Graph(std::size_t vertices) : storage(vertices) {}
  Storage storage;
  std::uint64_t version{0};
};

std::size_t vx_vertex_count(const Graph& graph) { return boost::num_vertices(graph.storage); }
bool vx_is_directed(const Graph&) { return false; }
std::uint64_t vx_version(const Graph& graph) { return graph.version; }

template <class Fn>
void vx_for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  const auto [begin, end] = boost::adjacent_vertices(u, graph.storage);
  for (auto it = begin; it != end; ++it) fn(static_cast<VertexId>(*it));
}

bool vx_has_edge(const Graph& graph, VertexId u, VertexId v) {
  return boost::edge(u, v, graph.storage).second;
}

void vx_apply_updates(Graph& graph, const UpdateBatch& batch) {
  if (batch.empty()) return;
  for (const auto& op : batch.updates) {
    if (op.src >= boost::num_vertices(graph.storage) ||
        op.dst >= boost::num_vertices(graph.storage)) {
      continue;
    }
    if (op.add) {
      if (!boost::edge(op.src, op.dst, graph.storage).second) {
        boost::add_edge(op.src, op.dst, graph.storage);
      }
    } else {
      boost::remove_edge(op.src, op.dst, graph.storage);
    }
  }
  ++graph.version;
}

}  // namespace boost_adapter

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
      const auto v = static_cast<VertexId>((static_cast<std::size_t>(u) + offset) % vertices);
      if (u == v) continue;
      const auto edge = std::minmax(u, v);
      unique.insert({edge.first, edge.second});
    }
  }
  return {unique.begin(), unique.end()};
}

std::vector<UpdateBatch> make_incremental_batches(std::size_t vertices) {
  std::vector<UpdateBatch> batches;
  batches.reserve(5);
  for (VertexId i = 0; i < 5; ++i) {
    UpdateBatch batch;
    const auto u = static_cast<VertexId>((i * 17) % vertices);
    const auto ring = static_cast<VertexId>((static_cast<std::size_t>(u) + 1) % vertices);
    const auto chord = static_cast<VertexId>((static_cast<std::size_t>(u) + 3) % vertices);
    batch.remove(std::min(u, ring), std::max(u, ring), i * 2);
    batch.add(std::min(u, chord), std::max(u, chord), i * 2 + 1);
    batches.push_back(std::move(batch));
  }
  return batches;
}

template <class Graph>
double median_recompute_us(Graph& graph, VertexId source, std::vector<std::uint32_t>& distances) {
  BasicIncrementalBFS<Graph> bfs(graph, source);
  std::vector<double> samples;
  samples.reserve(5);
  for (int rep = 0; rep < 5; ++rep) {
    const auto begin = std::chrono::steady_clock::now();
    bfs.recompute();
    const auto end = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
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
  boost_adapter::Graph boost_graph(vertices);
  for (const auto& [u, v] : edges) boost::add_edge(u, v, boost_graph.storage);

  std::vector<std::uint32_t> dynamic_dist;
  std::vector<std::uint32_t> csr_dist;
  std::vector<std::uint32_t> boost_dist;
  const auto dynamic_us = median_recompute_us(dynamic, source, dynamic_dist);
  const auto csr_us = median_recompute_us(csr, source, csr_dist);
  const auto boost_us = median_recompute_us(boost_graph, source, boost_dist);
  const bool recompute_exact = dynamic_dist == csr_dist && dynamic_dist == boost_dist;

  BasicIncrementalBFS<DynamicGraph> dynamic_incremental(dynamic, source);
  BasicIncrementalBFS<boost_adapter::Graph> boost_incremental(boost_graph, source);
  std::vector<double> dynamic_apply_samples;
  std::vector<double> boost_apply_samples;
  bool incremental_exact = true;
  for (const auto& batch : make_incremental_batches(vertices)) {
    auto begin = std::chrono::steady_clock::now();
    dynamic_incremental.apply(batch);
    auto end = std::chrono::steady_clock::now();
    dynamic_apply_samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());

    begin = std::chrono::steady_clock::now();
    boost_incremental.apply(batch);
    end = std::chrono::steady_clock::now();
    boost_apply_samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());

    incremental_exact = incremental_exact &&
        dynamic_incremental.distances() == boost_incremental.distances();
  }
  std::sort(dynamic_apply_samples.begin(), dynamic_apply_samples.end());
  std::sort(boost_apply_samples.begin(), boost_apply_samples.end());
  const auto dynamic_apply_us = dynamic_apply_samples[dynamic_apply_samples.size() / 2];
  const auto boost_apply_us = boost_apply_samples[boost_apply_samples.size() / 2];
  const bool exact = recompute_exact && incremental_exact;

  std::cout << std::fixed << std::setprecision(3)
            << "{\"schema_version\":1,"
            << "\"benchmark\":\"same-algorithm-storage-bfs\","
            << "\"backend\":\"boost-adjacency-list\","
            << "\"algorithm\":\"BasicIncrementalBFS\","
            << "\"vertices\":" << vertices << ','
            << "\"edges\":" << edges.size() << ','
            << "\"source\":" << source << ','
            << "\"repetitions\":5,"
            << "\"incremental_batches\":5,"
            << "\"exact\":" << (exact ? "true" : "false") << ','
            << "\"recompute_exact\":" << (recompute_exact ? "true" : "false") << ','
            << "\"incremental_exact\":" << (incremental_exact ? "true" : "false") << ','
            << "\"digest\":" << digest(dynamic_incremental.distances()) << ','
            << "\"dynamic_graph_median_us\":" << dynamic_us << ','
            << "\"csr_graph_median_us\":" << csr_us << ','
            << "\"boost_median_us\":" << boost_us << ','
            << "\"dynamic_incremental_median_us\":" << dynamic_apply_us << ','
            << "\"boost_incremental_median_us\":" << boost_apply_us
            << "}\n";

  return exact ? 0 : 2;
}
