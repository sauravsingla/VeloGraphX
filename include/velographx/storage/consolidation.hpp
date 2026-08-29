#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

struct ConsolidationSnapshot {
  DynamicGraph graph;
  std::size_t source_storage_bytes{0};
  std::size_t consolidated_storage_bytes{0};
  std::size_t directed_edges{0};
};

struct ConsolidationPolicy {
  double max_storage_growth_ratio{1.25};
  double max_neighbor_latency_ratio{1.25};
};

struct ConsolidationSignal {
  double storage_growth_ratio{1.0};
  double neighbor_latency_ratio{1.0};
  bool storage_limit_exceeded{false};
  bool latency_limit_exceeded{false};
  bool should_consolidate{false};
};

inline ConsolidationSignal evaluate_consolidation(
    std::size_t current_storage_bytes,
    std::size_t canonical_storage_bytes,
    double current_neighbor_latency,
    double canonical_neighbor_latency,
    ConsolidationPolicy policy = {}) noexcept {
  const auto storage_ratio = canonical_storage_bytes == 0
      ? 1.0
      : static_cast<double>(current_storage_bytes) /
            static_cast<double>(canonical_storage_bytes);
  const auto latency_ratio = canonical_neighbor_latency <= 0.0
      ? 1.0
      : current_neighbor_latency / canonical_neighbor_latency;
  const bool storage_exceeded = storage_ratio >= policy.max_storage_growth_ratio;
  const bool latency_exceeded = latency_ratio >= policy.max_neighbor_latency_ratio;
  return {storage_ratio, latency_ratio, storage_exceeded, latency_exceeded,
          storage_exceeded || latency_exceeded};
}

// Rebuild the current logical graph into a canonical segmented-CSR snapshot.
// This deliberately does not mutate the source graph: callers can validate the
// snapshot before an application-level cutover. Row patches and delta arenas in
// the returned graph are empty because bulk_load_edges() constructs fresh CSR.
inline ConsolidationSnapshot consolidate_to_csr_snapshot(const DynamicGraph& source) {
  std::vector<std::pair<VertexId, VertexId>> edges;
  edges.reserve(source.directed() ? source.edge_count_directed()
                                  : source.edge_count_directed() / 2);

  for (std::size_t u = 0; u < source.vertex_count(); ++u) {
    const auto row = source.neighbors(static_cast<VertexId>(u));
    for (const auto v : row) {
      if (source.directed() || u < static_cast<std::size_t>(v)) {
        edges.emplace_back(static_cast<VertexId>(u), v);
      }
    }
  }

  DynamicGraph consolidated(source.vertex_count(), source.directed());
  consolidated.bulk_load_edges(edges);
  const auto source_bytes = source.storage_bytes();
  const auto consolidated_bytes = consolidated.storage_bytes();
  const auto directed_edges = source.edge_count_directed();
  return {std::move(consolidated), source_bytes, consolidated_bytes, directed_edges};
}

}  // namespace velographx
