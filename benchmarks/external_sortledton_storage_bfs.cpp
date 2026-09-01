#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "data-structure/TransactionManager.h"
#include "data-structure/VersionedBlockedEdgeIterator.h"
#include "data-structure/VersioningBlockedSkipListAdjacencyList.h"
#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace sortledton_adapter {
using velographx::UpdateBatch;
using velographx::VertexId;

[[noreturn]] void throw_stage(const std::string& stage) {
  throw std::runtime_error("Sortledton stage failed: " + stage);
}

class Graph {
 public:
  Graph(std::size_t vertices, const std::vector<std::pair<VertexId, VertexId>>& edges)
      : vertices_(vertices), manager_(1), storage_(512, sizeof(double), manager_) {
    try { manager_.register_thread(0); } catch (...) { throw_stage("register_thread(0)"); }
    registered_ = true;
    for (VertexId v = 0; v < vertices_; ++v) insert_vertex(v);
    for (const auto& [u, v] : edges) insert_edge(u, v);
  }

  ~Graph() noexcept {
    if (!registered_) return;
    try { manager_.deregister_thread(0); } catch (...) {}
  }

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  std::size_t vertices_{0};
  mutable TransactionManager manager_;
  mutable VersioningBlockedSkipListAdjacencyList storage_;
  std::uint64_t version_{0};
  bool registered_{false};

 public:
  void insert_vertex(VertexId v) {
    SnapshotTransaction tx = [&]() {
      try { return manager_.getSnapshotTransaction(&storage_, true); }
      catch (...) { throw_stage("insert_vertex getSnapshotTransaction v=" + std::to_string(v)); }
    }();
    bool completed = false;
    try {
      try { tx.insert_vertex(v); }
      catch (...) { throw_stage("insert_vertex enqueue v=" + std::to_string(v)); }
      bool ok = false;
      try { ok = tx.execute(); }
      catch (...) { throw_stage("insert_vertex execute v=" + std::to_string(v)); }
      try { manager_.transactionCompleted(tx); completed = true; }
      catch (...) { throw_stage("insert_vertex transactionCompleted v=" + std::to_string(v)); }
      if (!ok) throw std::runtime_error("Sortledton vertex insertion returned false v=" + std::to_string(v));
    } catch (...) {
      if (!completed) { try { manager_.transactionCompleted(tx); } catch (...) {} }
      throw;
    }
  }

  void insert_edge(VertexId u, VertexId v) {
    SnapshotTransaction tx = [&]() {
      try { return manager_.getSnapshotTransaction(&storage_, true); }
      catch (...) { throw_stage("insert_edge getSnapshotTransaction " + std::to_string(u) + "->" + std::to_string(v)); }
    }();
    bool completed = false;
    try {
      const edge_t edge{static_cast<dst_t>(u), static_cast<dst_t>(v)};
      try {
        tx.use_vertex_does_not_exists_semantics();
        tx.insert_vertex(edge.src);
        tx.insert_vertex(edge.dst);
        double weight = 1.0;
        tx.insert_edge(edge, reinterpret_cast<char*>(&weight), sizeof(weight));
        tx.insert_edge({edge.dst, edge.src}, reinterpret_cast<char*>(&weight), sizeof(weight));
      } catch (...) { throw_stage("insert_edge enqueue " + std::to_string(u) + "->" + std::to_string(v)); }
      bool ok = false;
      try { ok = tx.execute(); }
      catch (...) { throw_stage("insert_edge execute " + std::to_string(u) + "->" + std::to_string(v)); }
      try { manager_.transactionCompleted(tx); completed = true; }
      catch (...) { throw_stage("insert_edge transactionCompleted " + std::to_string(u) + "->" + std::to_string(v)); }
      if (!ok) throw std::runtime_error("Sortledton edge insertion returned false " + std::to_string(u) + "->" + std::to_string(v));
    } catch (...) {
      if (!completed) { try { manager_.transactionCompleted(tx); } catch (...) {} }
      throw;
    }
  }

  void delete_edge(VertexId u, VertexId v) {
    SnapshotTransaction tx = [&]() {
      try { return manager_.getSnapshotTransaction(&storage_, true); }
      catch (...) { throw_stage("delete_edge getSnapshotTransaction " + std::to_string(u) + "->" + std::to_string(v)); }
    }();
    bool completed = false;
    try {
      const edge_t edge{static_cast<dst_t>(u), static_cast<dst_t>(v)};
      try {
        tx.delete_edge(edge);
        tx.delete_edge({edge.dst, edge.src});
      } catch (...) { throw_stage("delete_edge enqueue " + std::to_string(u) + "->" + std::to_string(v)); }
      bool ok = false;
      try { ok = tx.execute(); }
      catch (...) { throw_stage("delete_edge execute " + std::to_string(u) + "->" + std::to_string(v)); }
      try { manager_.transactionCompleted(tx); completed = true; }
      catch (...) { throw_stage("delete_edge transactionCompleted " + std::to_string(u) + "->" + std::to_string(v)); }
      if (!ok) throw std::runtime_error("Sortledton edge deletion returned false " + std::to_string(u) + "->" + std::to_string(v));
    } catch (...) {
      if (!completed) { try { manager_.transactionCompleted(tx); } catch (...) {} }
      throw;
    }
  }
};

std::size_t vx_vertex_count(const Graph& graph) { return graph.vertices_; }
bool vx_is_directed(const Graph&) { return false; }
std::uint64_t vx_version(const Graph& graph) { return graph.version_; }

template <class Fn>
void vx_for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  auto tx = graph.manager_.getSnapshotTransaction(&graph.storage_, false);
  bool completed = false;
  try {
    if (!tx.has_vertex(u)) {
      graph.manager_.transactionCompleted(tx);
      return;
    }
    const auto physical = tx.physical_id(u);
    auto iter = tx.neighbourhood_blocked_p(physical);
    while (iter.has_next_block()) {
      auto [versioned, begin, end] = iter.next_block();
      if (versioned) {
        while (iter.has_next_edge()) fn(static_cast<VertexId>(tx.logical_id(iter.next())));
      } else {
        for (auto it = begin; it < end; ++it) fn(static_cast<VertexId>(tx.logical_id(*it)));
      }
    }
    graph.manager_.transactionCompleted(tx);
    completed = true;
  } catch (...) {
    if (!completed) { try { graph.manager_.transactionCompleted(tx); } catch (...) {} }
    throw;
  }
}

bool vx_has_edge(const Graph& graph, VertexId u, VertexId v) {
  auto tx = graph.manager_.getSnapshotTransaction(&graph.storage_, false);
  bool completed = false;
  try {
    bool result = false;
    if (tx.has_vertex(u) && tx.has_vertex(v)) {
      const auto pu = tx.physical_id(u);
      const auto pv = tx.physical_id(v);
      result = tx.has_edge_p(edge_t{static_cast<dst_t>(pu), static_cast<dst_t>(pv)});
    }
    graph.manager_.transactionCompleted(tx);
    completed = true;
    return result;
  } catch (...) {
    if (!completed) { try { graph.manager_.transactionCompleted(tx); } catch (...) {} }
    throw;
  }
}

void vx_apply_updates(Graph& graph, const UpdateBatch& batch) {
  if (batch.empty()) return;
  for (const auto& op : batch.updates) {
    if (op.src >= graph.vertices_ || op.dst >= graph.vertices_) continue;
    const bool exists = vx_has_edge(graph, op.src, op.dst);
    if (op.add) {
      if (!exists) graph.insert_edge(op.src, op.dst);
    } else if (exists) {
      graph.delete_edge(op.src, op.dst);
    }
  }
  ++graph.version_;
}
}  // namespace sortledton_adapter

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

std::vector<UpdateBatch> make_batches(std::size_t vertices) {
  std::vector<UpdateBatch> batches;
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

int run(std::size_t vertices) {
  const VertexId source = 0;
  const auto edges = make_graph(vertices);
  DynamicGraph dynamic(vertices, false);
  dynamic.bulk_load_edges(edges);
  CsrGraph csr(edges, false);
  std::vector<std::uint32_t> dynamic_dist, csr_dist;
  const auto dynamic_us = median_recompute_us(dynamic, source, dynamic_dist);
  const auto csr_us = median_recompute_us(csr, source, csr_dist);
  if (dynamic_dist != csr_dist) throw std::runtime_error("native DynamicGraph/CSR oracle mismatch");

  sortledton_adapter::Graph sortledton(vertices, edges);
  std::vector<std::uint32_t> sortledton_dist;
  const auto sortledton_us = median_recompute_us(sortledton, source, sortledton_dist);
  const bool recompute_exact = dynamic_dist == sortledton_dist;

  BasicIncrementalBFS<DynamicGraph> dynamic_inc(dynamic, source);
  BasicIncrementalBFS<sortledton_adapter::Graph> sortledton_inc(sortledton, source);
  std::vector<double> dynamic_samples, sortledton_samples;
  bool incremental_exact = true;
  for (const auto& batch : make_batches(vertices)) {
    auto begin = std::chrono::steady_clock::now(); dynamic_inc.apply(batch); auto end = std::chrono::steady_clock::now();
    dynamic_samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
    begin = std::chrono::steady_clock::now(); sortledton_inc.apply(batch); end = std::chrono::steady_clock::now();
    sortledton_samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
    incremental_exact = incremental_exact && dynamic_inc.distances() == sortledton_inc.distances();
  }
  std::sort(dynamic_samples.begin(), dynamic_samples.end());
  std::sort(sortledton_samples.begin(), sortledton_samples.end());
  const bool exact = recompute_exact && incremental_exact;
  std::cout << std::fixed << std::setprecision(3)
            << "{\"schema_version\":1,\"benchmark\":\"same-algorithm-storage-bfs\",\"backend\":\"sortledton\",\"algorithm\":\"BasicIncrementalBFS\",\"claim_scope\":\"correctness-and-storage-portability-only\",\"publication_grade\":false,"
            << "\"vertices\":" << vertices << ",\"edges\":" << edges.size() << ",\"source\":" << source
            << ",\"repetitions\":5,\"incremental_batches\":5,\"exact\":" << (exact ? "true" : "false")
            << ",\"recompute_exact\":" << (recompute_exact ? "true" : "false") << ",\"incremental_exact\":" << (incremental_exact ? "true" : "false")
            << ",\"digest\":" << digest(dynamic_inc.distances()) << ",\"dynamic_graph_median_us\":" << dynamic_us
            << ",\"csr_graph_median_us\":" << csr_us << ",\"sortledton_median_us\":" << sortledton_us
            << ",\"dynamic_incremental_median_us\":" << dynamic_samples[dynamic_samples.size()/2]
            << ",\"sortledton_incremental_median_us\":" << sortledton_samples[sortledton_samples.size()/2] << "}\n";
  return exact ? 0 : 2;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const std::size_t vertices = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 8192;
    return run(vertices);
  } catch (const std::exception& e) {
    std::cerr << "Sortledton adapter error: " << e.what() << '\n';
    return 70;
  } catch (...) {
    std::cerr << "Sortledton adapter error: non-standard upstream exception\n";
    return 71;
  }
}
