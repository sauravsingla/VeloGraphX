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
using velographx::ConsolidationPolicy;
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
    for (const auto v : row) {
      h ^= static_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    }
  }
  return h;
}

std::pair<double, std::uint64_t> probe_once(const DynamicGraph& g, std::size_t count) {
  std::uint64_t h = 1469598103934665603ULL;
  const auto begin = Clock::now();
  for (std::size_t i = 0; i < count; ++i) {
    const auto u = static_cast<VertexId>((i * 11400714819323198485ULL) % g.vertex_count());
    const auto row = g.neighbors(u);
    h ^= static_cast<std::uint64_t>(row.size()) + (h << 6) + (h >> 2);
    for (const auto v : row) {
      h ^= static_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    }
  }
  const auto end = Clock::now();
  const auto ns = std::chrono::duration<double, std::nano>(end - begin).count();
  return {ns / static_cast<double>(count), h};
}

std::pair<double, std::uint64_t> probe_median(const DynamicGraph& g, std::size_t count,
                                               std::size_t repeats) {
  std::vector<double> values;
  values.reserve(repeats);
  std::uint64_t expected_digest = 0;
  for (std::size_t i = 0; i < repeats; ++i) {
    const auto [latency, digest] = probe_once(g, count);
    if (i == 0) expected_digest = digest;
    if (digest != expected_digest) throw std::runtime_error("neighbor probe digest changed within checkpoint");
    values.push_back(latency);
  }
  std::sort(values.begin(), values.end());
  return {values[values.size() / 2], expected_digest};
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
    for (std::size_t tries = 0; tries < 64 &&
         (new_v == u || g.has_edge(u, new_v)); ++tries) {
      new_v = static_cast<VertexId>((static_cast<std::uint64_t>(new_v) + 7919) % n);
    }
    if (new_v == u || g.has_edge(u, new_v)) continue;
    batch.remove(u, old_v);
    batch.add(u, new_v);
  }
  return batch;
}

struct EpochRecord {
  std::size_t epoch{0};
  std::size_t update_operations{0};
  double update_ms{0.0};
  double compact_ms{0.0};
  std::size_t storage_bytes{0};
  double storage_ratio{1.0};
  double neighbor_ns{0.0};
  double latency_ratio{1.0};
  bool consolidation_triggered{false};
  double consolidation_ms{0.0};
  double post_storage_ratio{1.0};
  double post_latency_ratio{1.0};
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 8) {
    std::cerr << "usage: velographx_steady_state_storage <edge-list> [epochs=60] [rows-per-epoch=4096] [probes=4096] [directed=1] [storage-threshold=1.25] [latency-threshold=1.25]\n";
    return 2;
  }

  try {
    const std::string path = argv[1];
    const std::size_t epochs = argc >= 3 ? std::stoull(argv[2]) : 60;
    const std::size_t rows_per_epoch = argc >= 4 ? std::stoull(argv[3]) : 4096;
    const std::size_t probes = argc >= 5 ? std::stoull(argv[4]) : 4096;
    const bool directed = argc >= 6 ? std::stoi(argv[5]) != 0 : true;
    const double storage_threshold = argc >= 7 ? std::stod(argv[6]) : 1.25;
    const double latency_threshold = argc >= 8 ? std::stod(argv[7]) : 1.25;
    if (epochs == 0 || rows_per_epoch == 0 || probes == 0) {
      throw std::runtime_error("epochs, rows-per-epoch and probes must be positive");
    }

    auto edges = read_edges(path);
    VertexId max_v = 0;
    for (const auto& [u, v] : edges) max_v = std::max(max_v, std::max(u, v));
    DynamicGraph graph(static_cast<std::size_t>(max_v) + 1, directed);
    graph.bulk_load_edges(edges);
    std::vector<std::pair<VertexId, VertexId>>().swap(edges);

    const auto initial_directed_edges = graph.edge_count_directed();
    std::size_t canonical_storage = graph.storage_bytes();
    auto [canonical_neighbor_ns, canonical_probe_digest] = probe_median(graph, probes, 5);
    const auto initial_storage = canonical_storage;
    const auto initial_neighbor_ns = canonical_neighbor_ns;
    const ConsolidationPolicy policy{storage_threshold, latency_threshold};

    std::vector<EpochRecord> records;
    records.reserve(epochs);
    std::size_t total_update_operations = 0;
    double total_update_ms = 0.0;
    double total_compact_ms = 0.0;
    double total_consolidation_ms = 0.0;
    std::size_t consolidation_count = 0;
    std::size_t storage_trigger_count = 0;
    std::size_t latency_trigger_count = 0;
    std::size_t high_water_storage_bytes = canonical_storage;
    double high_water_storage_ratio = 1.0;
    double high_water_latency_ratio = 1.0;
    double worst_post_consolidation_latency_ratio = 1.0;
    double worst_post_consolidation_storage_ratio = 1.0;
    std::size_t last_consolidation_epoch = 0;
    std::vector<std::size_t> consolidation_intervals;

    for (std::size_t epoch = 1; epoch <= epochs; ++epoch) {
      auto batch = make_mutation_round(graph, epoch - 1, rows_per_epoch);
      const auto update_operations = batch.updates.size();
      total_update_operations += update_operations;

      const auto u0 = Clock::now();
      graph.apply(batch);
      const auto u1 = Clock::now();
      const auto c0 = Clock::now();
      graph.compact();
      const auto c1 = Clock::now();
      const double update_ms = std::chrono::duration<double, std::milli>(u1 - u0).count();
      const double compact_ms = std::chrono::duration<double, std::milli>(c1 - c0).count();
      total_update_ms += update_ms;
      total_compact_ms += compact_ms;

      const auto storage = graph.storage_bytes();
      const auto [neighbor_ns, probe_digest] = probe_median(graph, probes, 3);
      const auto signal = velographx::evaluate_consolidation(
          storage, canonical_storage, neighbor_ns, canonical_neighbor_ns, policy);

      high_water_storage_bytes = std::max(high_water_storage_bytes, storage);
      high_water_storage_ratio = std::max(high_water_storage_ratio, signal.storage_growth_ratio);
      high_water_latency_ratio = std::max(high_water_latency_ratio, signal.neighbor_latency_ratio);

      EpochRecord record;
      record.epoch = epoch;
      record.update_operations = update_operations;
      record.update_ms = update_ms;
      record.compact_ms = compact_ms;
      record.storage_bytes = storage;
      record.storage_ratio = signal.storage_growth_ratio;
      record.neighbor_ns = neighbor_ns;
      record.latency_ratio = signal.neighbor_latency_ratio;

      if (signal.should_consolidate) {
        if (signal.storage_limit_exceeded) ++storage_trigger_count;
        if (signal.latency_limit_exceeded) ++latency_trigger_count;
        const auto before_digest = digest_graph(graph);
        const auto before_edges = graph.edge_count_directed();
        const auto s0 = Clock::now();
        auto snapshot = velographx::consolidate_to_csr_snapshot(graph);
        const auto s1 = Clock::now();
        const double consolidation_ms = std::chrono::duration<double, std::milli>(s1 - s0).count();
        const auto after_digest = digest_graph(snapshot.graph);
        const auto [post_neighbor_ns, post_probe_digest] = probe_median(snapshot.graph, probes, 5);
        if (before_digest != after_digest || before_edges != snapshot.graph.edge_count_directed() ||
            probe_digest != post_probe_digest) {
          throw std::runtime_error("steady-state consolidation correctness gate failed");
        }

        record.consolidation_triggered = true;
        record.consolidation_ms = consolidation_ms;
        record.post_storage_ratio = storage == 0 ? 1.0
            : static_cast<double>(snapshot.consolidated_storage_bytes) / static_cast<double>(storage);
        record.post_latency_ratio = neighbor_ns <= 0.0 ? 1.0 : post_neighbor_ns / neighbor_ns;
        worst_post_consolidation_storage_ratio = std::max(worst_post_consolidation_storage_ratio,
                                                          record.post_storage_ratio);
        worst_post_consolidation_latency_ratio = std::max(worst_post_consolidation_latency_ratio,
                                                          record.post_latency_ratio);
        total_consolidation_ms += consolidation_ms;
        ++consolidation_count;
        consolidation_intervals.push_back(epoch - last_consolidation_epoch);
        last_consolidation_epoch = epoch;

        graph = std::move(snapshot.graph);
        canonical_storage = graph.storage_bytes();
        canonical_neighbor_ns = post_neighbor_ns;
        canonical_probe_digest = post_probe_digest;
      }
      records.push_back(record);
    }

    // Final correctness validation is intentionally not counted as maintenance cost.
    const auto final_before_digest = digest_graph(graph);
    const auto final_before_edges = graph.edge_count_directed();
    const auto v0 = Clock::now();
    auto final_snapshot = velographx::consolidate_to_csr_snapshot(graph);
    const auto v1 = Clock::now();
    const auto final_after_digest = digest_graph(final_snapshot.graph);
    const auto [final_snapshot_neighbor_ns, final_snapshot_probe_digest] =
        probe_median(final_snapshot.graph, probes, 3);
    (void)final_snapshot_neighbor_ns;
    if (final_before_digest != final_after_digest ||
        final_before_edges != final_snapshot.graph.edge_count_directed() ||
        canonical_probe_digest == 0 || final_snapshot_probe_digest == 0) {
      throw std::runtime_error("final steady-state correctness validation failed");
    }
    const double final_validation_ms =
        std::chrono::duration<double, std::milli>(v1 - v0).count();

    const double update_seconds = total_update_ms / 1000.0;
    const double maintenance_seconds = (total_update_ms + total_compact_ms + total_consolidation_ms) / 1000.0;
    const double update_throughput = update_seconds > 0.0
        ? static_cast<double>(total_update_operations) / update_seconds : 0.0;
    const double amortized_throughput = maintenance_seconds > 0.0
        ? static_cast<double>(total_update_operations) / maintenance_seconds : 0.0;
    const double consolidation_share = (total_update_ms + total_compact_ms + total_consolidation_ms) > 0.0
        ? total_consolidation_ms / (total_update_ms + total_compact_ms + total_consolidation_ms) : 0.0;

    std::cout << "{"
              << "\"artifact_type\":\"velographx-steady-state-storage\","
              << "\"schema_version\":1,"
              << "\"research_claim\":false,"
              << "\"directed\":" << (directed ? "true" : "false") << ','
              << "\"vertices\":" << graph.vertex_count() << ','
              << "\"initial_directed_edges\":" << initial_directed_edges << ','
              << "\"final_directed_edges\":" << final_before_edges << ','
              << "\"epochs\":" << epochs << ','
              << "\"rows_per_epoch\":" << rows_per_epoch << ','
              << "\"probes\":" << probes << ','
              << "\"storage_threshold\":" << storage_threshold << ','
              << "\"latency_threshold\":" << latency_threshold << ','
              << "\"initial_storage_bytes\":" << initial_storage << ','
              << "\"high_water_storage_bytes\":" << high_water_storage_bytes << ','
              << "\"high_water_storage_ratio\":" << high_water_storage_ratio << ','
              << "\"initial_neighbor_ns\":" << initial_neighbor_ns << ','
              << "\"high_water_latency_ratio\":" << high_water_latency_ratio << ','
              << "\"total_update_operations\":" << total_update_operations << ','
              << "\"total_update_ms\":" << total_update_ms << ','
              << "\"total_row_compaction_ms\":" << total_compact_ms << ','
              << "\"total_consolidation_ms\":" << total_consolidation_ms << ','
              << "\"consolidation_count\":" << consolidation_count << ','
              << "\"storage_trigger_count\":" << storage_trigger_count << ','
              << "\"latency_trigger_count\":" << latency_trigger_count << ','
              << "\"consolidation_share\":" << consolidation_share << ','
              << "\"update_only_ops_per_s\":" << update_throughput << ','
              << "\"amortized_ops_per_s\":" << amortized_throughput << ','
              << "\"final_validation_ms\":" << final_validation_ms << ','
              << "\"correct\":true,"
              << "\"consolidation_intervals\":[";
    for (std::size_t i = 0; i < consolidation_intervals.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << consolidation_intervals[i];
    }
    std::cout << "],\"epochs_detail\":[";
    for (std::size_t i = 0; i < records.size(); ++i) {
      if (i) std::cout << ',';
      const auto& r = records[i];
      std::cout << '{'
                << "\"epoch\":" << r.epoch << ','
                << "\"update_operations\":" << r.update_operations << ','
                << "\"update_ms\":" << r.update_ms << ','
                << "\"compact_ms\":" << r.compact_ms << ','
                << "\"storage_ratio\":" << r.storage_ratio << ','
                << "\"neighbor_ns\":" << r.neighbor_ns << ','
                << "\"latency_ratio\":" << r.latency_ratio << ','
                << "\"consolidated\":" << (r.consolidation_triggered ? "true" : "false") << ','
                << "\"consolidation_ms\":" << r.consolidation_ms << ','
                << "\"post_storage_ratio\":" << r.post_storage_ratio << ','
                << "\"post_latency_ratio\":" << r.post_latency_ratio
                << '}';
    }
    std::cout << "]}\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
