#pragma once

#include "velographx/runtime/numa.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace velographx {

inline std::vector<std::pair<std::size_t, std::size_t>> contiguous_partitions(
    std::size_t n, std::size_t parts) {
  parts = std::max<std::size_t>(1, std::min(parts, std::max<std::size_t>(1, n)));
  std::vector<std::pair<std::size_t, std::size_t>> out;
  out.reserve(parts);
  for (std::size_t p = 0; p < parts; ++p) {
    const auto begin = n * p / parts;
    const auto end = n * (p + 1) / parts;
    out.push_back({begin, end});
  }
  return out;
}

struct NumaVertexPartition {
  std::size_t partition_id{0};
  std::size_t vertex_begin{0};
  std::size_t vertex_end{0};
  std::optional<std::size_t> node_id;
  std::vector<std::size_t> local_cpus;
};

inline std::vector<NumaVertexPartition> plan_numa_vertex_partitions(
    std::size_t vertex_count, const NumaInfo& info, NumaMode mode,
    std::size_t partitions = 0) {
  const std::size_t topology_nodes = info.topology.empty() ? 1 : info.topology.size();
  if (partitions == 0) partitions = topology_nodes;
  const auto ranges = contiguous_partitions(vertex_count, partitions);

  std::vector<NumaVertexPartition> result;
  result.reserve(ranges.size());
  for (std::size_t i = 0; i < ranges.size(); ++i) {
    NumaVertexPartition placement;
    placement.partition_id = i;
    placement.vertex_begin = ranges[i].first;
    placement.vertex_end = ranges[i].second;
    if (mode != NumaMode::off && !info.topology.empty()) {
      const auto& node = info.topology[i % info.topology.size()];
      placement.node_id = node.id;
      placement.local_cpus = node.cpus;
    }
    result.push_back(std::move(placement));
  }
  return result;
}

inline std::optional<std::size_t> numa_node_for_vertex(
    std::size_t vertex, const std::vector<NumaVertexPartition>& partitions) noexcept {
  for (const auto& partition : partitions) {
    if (vertex >= partition.vertex_begin && vertex < partition.vertex_end)
      return partition.node_id;
  }
  return std::nullopt;
}

}  // namespace velographx
