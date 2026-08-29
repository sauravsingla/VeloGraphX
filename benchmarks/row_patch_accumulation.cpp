#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "velographx/storage/consolidation.hpp"

namespace {
using Clock = std::chrono::steady_clock;
using velographx::DynamicGraph;
using velographx::UpdateBatch;
using velographx::VertexId;

std::vector<std::pair<VertexId, VertexId>> read_edges(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list");
  std::vector<std::pair<VertexId, VertexId>> edges;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '%') continue;
    std::istringstream ss(line);
    std::uint64_t u = 0, v = 0;
    if (!(ss >> u >> v)) continue;
    if (u == v || u > std::numeric_limits<VertexId>::max() ||
        v > std::numeric_limits<VertexId>::max()) continue;
    edges.emplace_back(static_cast<VertexId>(u), static_cast<VertexId>(v));
  }
  if (edges.empty()) throw std::runtime_error("empty edge list");
  return edges;
}

std::uint64_t digest_graph(const DynamicGraph& g) {
  std::uint64_t h = 1469598103934665603ULL;
  for (std::size_t u = 0; u < g.vertex_count(); ++u) {
    const auto row = g.neighbors(static_cast<VertexId>(u));
    h ^= static_cast<std::uint64_t>(u) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint64_t>(row.size()) + (h << 6) + (h >> 2);
    for (const auto v : row) h ^= static_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  }
  return h;
}

std::pair<double, std::uint64_t> probe(const DynamicGraph& g, std::size_t count) {
  std::uint64_t h = 1469598103934665603ULL;
  const auto begin = Clock::now();
  for (std::size_t i = 0; i < count; ++i) {
    const auto u = static_cast<VertexId>((i * 11400714819323198485ULL) % g.vertex_count());
    const auto row = g.neighbors(u);
    h ^= static_cast<std::uint64_t>(row.size()) + (h << 6) + (h >> 2);
    for (const auto v : row) h ^= static_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  }
  const auto end = Clock::now();
  const auto ns = std::chrono::duration<double, std::nano>(end - begin).count();
  return {ns / static_cast<double>(count), h};
}

UpdateBatch make_mutation_round(const DynamicGraph& g, std::size_t round,
                                std::size_t changed_rows) {
  UpdateBatch batch;
  batch.updates.reserve(changed_rows * 2);
  const auto n = g.vertex_count();
  for (std::size_t i = 0; i < changed_rows; ++i) {
    const auto u = static_cast<VertexId>((round * 104729ULL + i * 8191ULL) % n);
    const auto row = g.neighbors(u);
    if (row.empty()) continue;
    const auto old_v = row[(round + i) % row.size()];
    VertexId new_v = static_cast<VertexId>((static_cast<std::uint64_t>(old_v) + 97 + round + i) % n);
    for (std::size_t tries = 0; tries < 32 &&
         (new_v == u || g.has_edge(u, new_v)); ++tries) {
      new_v = static_cast<VertexId>((static_cast<std::uint64_t>(new_v) + 7919) % n);
    }
    if (new_v == u || g.has_edge(u, new_v)) continue;
    batch.remove(u, old_v);
    batch.add(u, new_v);
  }
  return batch;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 5) {
    std::cerr << "usage: velographx_row_patch_accumulation <edge-list> [cycles=20] [rows-per-cycle=64] [probes=1024]\n";
    return 2;
  }
  try {
    const std::string path = argv[1];
    const std::size_t cycles = argc >= 3 ? std::stoull(argv[2]) : 20;
    const std::size_t rows_per_cycle = argc >= 4 ? std::stoull(argv[3]) : 64;
    const std::size_t probes = argc >= 5 ? std::stoull(argv[4]) : 1024;

    auto edges = read_edges(path);
    VertexId max_v = 0;
    for (const auto& [u, v] : edges) max_v = std::max(max_v, std::max(u, v));
    DynamicGraph graph(static_cast<std::size_t>(max_v) + 1, false);
    graph.bulk_load_edges(edges);
    const auto initial_storage = graph.storage_bytes();
    const auto initial_edges = graph.edge_count_directed();
    const auto [initial_probe_ns, initial_probe_digest] = probe(graph, probes);

    double total_update_ms = 0.0;
    double total_compact_ms = 0.0;
    std::size_t applied_updates = 0;
    for (std::size_t round = 0; round < cycles; ++round) {
      auto batch = make_mutation_round(graph, round, rows_per_cycle);
      applied_updates += batch.updates.size();
      const auto u0 = Clock::now();
      graph.apply(batch);
      const auto u1 = Clock::now();
      const auto c0 = Clock::now();
      graph.compact();
      const auto c1 = Clock::now();
      total_update_ms += std::chrono::duration<double, std::milli>(u1 - u0).count();
      total_compact_ms += std::chrono::duration<double, std::milli>(c1 - c0).count();
    }

    const auto accumulated_storage = graph.storage_bytes();
    const auto [before_ns, before_probe_digest] = probe(graph, probes);
    const auto before_digest = digest_graph(graph);
    const auto before_edges = graph.edge_count_directed();

    const auto s0 = Clock::now();
    auto snapshot = velographx::consolidate_to_csr_snapshot(graph);
    const auto s1 = Clock::now();
    const auto consolidation_ms = std::chrono::duration<double, std::milli>(s1 - s0).count();
    const auto [after_ns, after_probe_digest] = probe(snapshot.graph, probes);
    const auto after_digest = digest_graph(snapshot.graph);

    const bool correct = before_digest == after_digest &&
                         before_probe_digest == after_probe_digest &&
                         before_edges == snapshot.graph.edge_count_directed();
    if (!correct) throw std::runtime_error("consolidation correctness gate failed");

    const double storage_growth = initial_storage == 0 ? 0.0
        : static_cast<double>(accumulated_storage) / static_cast<double>(initial_storage);
    const double storage_recovery = accumulated_storage == 0 ? 0.0
        : static_cast<double>(snapshot.consolidated_storage_bytes) /
          static_cast<double>(accumulated_storage);

    std::cout << "{"
              << "\"artifact_type\":\"velographx-row-patch-accumulation\"," 
              << "\"research_claim\":false,"
              << "\"vertices\":" << graph.vertex_count() << ','
              << "\"initial_directed_edges\":" << initial_edges << ','
              << "\"final_directed_edges\":" << before_edges << ','
              << "\"cycles\":" << cycles << ','
              << "\"rows_per_cycle\":" << rows_per_cycle << ','
              << "\"applied_update_operations\":" << applied_updates << ','
              << "\"initial_storage_bytes\":" << initial_storage << ','
              << "\"accumulated_storage_bytes\":" << accumulated_storage << ','
              << "\"consolidated_storage_bytes\":" << snapshot.consolidated_storage_bytes << ','
              << "\"storage_growth_ratio\":" << storage_growth << ','
              << "\"post_consolidation_storage_ratio\":" << storage_recovery << ','
              << "\"initial_neighbor_ns\":" << initial_probe_ns << ','
              << "\"pre_consolidation_neighbor_ns\":" << before_ns << ','
              << "\"post_consolidation_neighbor_ns\":" << after_ns << ','
              << "\"total_update_ms\":" << total_update_ms << ','
              << "\"total_row_compaction_ms\":" << total_compact_ms << ','
              << "\"consolidation_ms\":" << consolidation_ms << ','
              << "\"initial_probe_digest\":" << initial_probe_digest << ','
              << "\"logical_digest\":" << before_digest << ','
              << "\"correct\":true}\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
