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

#include "data-structure/EdgeDoesNotExistsPrecondition.h"
#include "data-structure/TransactionManager.h"
#include "data-structure/VersionedBlockedEdgeIterator.h"
#include "data-structure/VersioningBlockedSkipListAdjacencyList.h"
#include "data-structure/VertexExistsPrecondition.h"
#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace sortledton_adapter {

using velographx::UpdateBatch;
using velographx::VertexId;

class Graph {
 public:
  Graph(std::size_t vertices, const std::vector<std::pair<VertexId, VertexId>>& edges)
      : vertices_(vertices), manager_(1), storage_(512, sizeof(double), manager_) {
    std::cerr << "[sortledton-adapter] register-thread\n";
    manager_.register_thread(0);
    registered_ = true;
    std::cerr << "[sortledton-adapter] load-vertices\n";
    for (VertexId v = 0; v < vertices_; ++v) insert_vertex(v);
    std::cerr << "[sortledton-adapter] load-edges count=" << edges.size() << "\n";
    for (const auto& [u, v] : edges) insert_edge(u, v);
    std::cerr << "[sortledton-adapter] ready\n";
  }

  ~Graph() noexcept {
    if (registered_) {
      try {
        manager_.deregister_thread(0);
      } catch (const std::exception& e) {
        std::cerr << "[sortledton-adapter] deregister failure: " << e.what() << '\n';
      } catch (...) {
        std::cerr << "[sortledton-adapter] deregister failure: unknown exception\n";
      }
    }
  }

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  std::size_t vertices_{0};
  mutable TransactionManager manager_;
  mutable VersioningBlockedSkipListAdjacencyList storage_;
  std::uint64_t version_{0};
  bool registered_{false};

  void insert_vertex(VertexId v) {
    auto tx = manager_.getSnapshotTransaction(&storage_, true);
    tx.insert_vertex(v);
    if (!tx.execute()) throw std::runtime_error("Sortledton vertex insertion failed at vertex " + std::to_string(v));
    manager_.transactionCompleted(tx);
  }

  void insert_edge(VertexId u, VertexId v) {
    auto tx = manager_.getSnapshotTransaction(&storage_, true);
    const edge_t edge{static_cast<dst_t>(u), static_cast<dst_t>(v)};
    VertexExistsPrecondition source_exists(edge.src);
    VertexExistsPrecondition destination_exists(edge.dst);
    EdgeDoesNotExistsPrecondition edge_absent(edge);
    tx.register_precondition(&source_exists);
    tx.register_precondition(&destination_exists);
    tx.register_precondition(&edge_absent);
    double weight = 1.0;
    tx.insert_edge(edge, reinterpret_cast<char*>(&weight), sizeof(weight));
    tx.insert_edge({edge.dst, edge.src}, reinterpret_cast<char*>(&weight), sizeof(weight));
    if (!tx.execute()) throw std::runtime_error("Sortledton edge insertion failed");
    manager_.transactionCompleted(tx);
  }

  void delete_edge(VertexId u, VertexId v) {
    auto tx = manager_.getSnapshotTransaction(&storage_, true);
    const edge_t edge{static_cast<dst_t>(u), static_cast<dst_t>(v)};
    tx.delete_edge(edge);
    tx.delete_edge({edge.dst, edge.src});
    if (!tx.execute()) throw std::runtime_error("Sortledton edge deletion failed");
    manager_.transactionCompleted(tx);
  }
};

std::size_t vx_vertex_count(const Graph& graph) { return graph.vertices_; }
bool vx_is_directed(const Graph&) { return false; }
std::uint64_t vx_version(const Graph& graph) { return graph.version_; }

template <class Fn>
void vx_for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  auto tx = graph.manager_.getSnapshotTransaction(&graph.storage_, false);
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
}

bool vx_has_edge(const Graph& graph, VertexId u, VertexId v) {
  auto tx = graph.manager_.getSnapshotTransaction(&graph.storage_, false);
  const bool result = tx.has_edge(edge_t{static_cast<dst_t>(u), static_cast<dst_t>(v)});
  graph.manager_.transactionCompleted(tx);
  return result;
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

int run_campaign(std::size_t vertices) {
  const VertexId source = 0;
  const auto edges = make_graph(vertices);

  // Establish the native oracle before constructing the external backend. This
  // prevents an external-storage fault from being misattributed to CSR and also
  // gives us immutable reference vectors for the exactness gate.
  std::cerr << "[sortledton-adapter] construct-native\n";
  DynamicGraph dynamic(vertices, false);
  dynamic.bulk_load_edges(edges);
  CsrGraph csr(edges, false);
  std::vector<std::uint32_t> dynamic_dist, csr_dist;
  std::cerr << "[sortledton-adapter] recompute-dynamic\n";
  const auto dynamic_us = median_recompute_us(dynamic, source, dynamic_dist);
  std::cerr << "[sortledton-adapter] recompute-csr\n";
  const auto csr_us = median_recompute_us(csr, source, csr_dist);
  if (dynamic_dist != csr_dist) throw std::runtime_error("native DynamicGraph/CSR oracle mismatch");
  std::cerr << "[sortledton-adapter] native-oracle-ready\n";

  std::cerr << "[sortledton-adapter] construct-sortledton\n";
  sortledton_adapter::Graph sortledton_graph(vertices, edges);
  std::vector<std::uint32_t> sortledton_dist;
  std::cerr << "[sortledton-adapter] recompute-sortledton\n";
  const auto sortledton_us = median_recompute_us(sortledton_graph, source, sortledton_dist);
  const bool recompute_exact = dynamic_dist == sortledton_dist;

  std::cerr << "[sortledton-adapter] incremental-construct\n";
  BasicIncrementalBFS<DynamicGraph> dynamic_incremental(dynamic, source);
  BasicIncrementalBFS<sortledton_adapter::Graph> sortledton_incremental(sortledton_graph, source);
  std::vector<double> dynamic_apply_samples, sortledton_apply_samples;
  bool incremental_exact = true;
  int batch_index = 0;
  for (const auto& batch : make_incremental_batches(vertices)) {
    std::cerr << "[sortledton-adapter] batch=" << batch_index << "\n";
    auto begin = std::chrono::steady_clock::now();
    dynamic_incremental.apply(batch);
    auto end = std::chrono::steady_clock::now();
    dynamic_apply_samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());

    begin = std::chrono::steady_clock::now();
    sortledton_incremental.apply(batch);
    end = std::chrono::steady_clock::now();
    sortledton_apply_samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
    incremental_exact = incremental_exact && dynamic_incremental.distances() == sortledton_incremental.distances();
    ++batch_index;
  }
  std::sort(dynamic_apply_samples.begin(), dynamic_apply_samples.end());
  std::sort(sortledton_apply_samples.begin(), sortledton_apply_samples.end());
  const auto dynamic_apply_us = dynamic_apply_samples[dynamic_apply_samples.size() / 2];
  const auto sortledton_apply_us = sortledton_apply_samples[sortledton_apply_samples.size() / 2];
  const bool exact = recompute_exact && incremental_exact;

  std::cout << std::fixed << std::setprecision(3)
            << "{\"schema_version\":1,"
            << "\"benchmark\":\"same-algorithm-storage-bfs\","
            << "\"backend\":\"sortledton\","
            << "\"algorithm\":\"BasicIncrementalBFS\","
            << "\"claim_scope\":\"correctness-and-storage-portability-only\","
            << "\"publication_grade\":false,"
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
            << "\"sortledton_median_us\":" << sortledton_us << ','
            << "\"dynamic_incremental_median_us\":" << dynamic_apply_us << ','
            << "\"sortledton_incremental_median_us\":" << sortledton_apply_us
            << "}\n" << std::flush;
  std::cerr << "[sortledton-adapter] result-emitted exact=" << exact << "\n";
  return exact ? 0 : 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::size_t vertices = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 8192;
    return run_campaign(vertices);
  } catch (const std::exception& e) {
    std::cerr << "[sortledton-adapter] fatal exception: " << e.what() << '\n';
    return 70;
  } catch (...) {
    std::cerr << "[sortledton-adapter] fatal unknown exception\n";
    return 71;
  }
}
