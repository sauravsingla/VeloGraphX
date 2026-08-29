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
  return {std::move(consolidated), source.storage_bytes(),
          consolidated.storage_bytes(), source.edge_count_directed()};
}

}  // namespace velographx
