#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace velographx {

enum class FrontierScheduleMode {
  vertex_balanced,
  edge_balanced,
  hybrid
};

struct FrontierScheduleDecision {
  FrontierScheduleMode mode{FrontierScheduleMode::vertex_balanced};
  std::size_t frontier_vertices{0};
  std::size_t frontier_edges{0};
  double average_degree{0.0};
  std::size_t recommended_grain{1};
};

inline FrontierScheduleDecision choose_frontier_schedule(
    const std::vector<std::size_t>& degrees,
    std::size_t workers,
    double dense_frontier_fraction = 0.08,
    std::size_t high_degree_threshold = 64) {
  FrontierScheduleDecision decision;
  decision.frontier_vertices = degrees.size();
  decision.frontier_edges = std::accumulate(degrees.begin(), degrees.end(), std::size_t{0});
  decision.average_degree = degrees.empty()
      ? 0.0
      : static_cast<double>(decision.frontier_edges) / static_cast<double>(degrees.size());

  if (workers == 0) workers = 1;
  const auto high_degree = static_cast<std::size_t>(std::count_if(
      degrees.begin(), degrees.end(), [high_degree_threshold](std::size_t degree) {
        return degree >= high_degree_threshold;
      }));
  const double high_degree_fraction = degrees.empty()
      ? 0.0
      : static_cast<double>(high_degree) / static_cast<double>(degrees.size());

  if (decision.average_degree >= static_cast<double>(high_degree_threshold) ||
      high_degree_fraction >= dense_frontier_fraction) {
    decision.mode = FrontierScheduleMode::edge_balanced;
  } else if (decision.average_degree >= static_cast<double>(high_degree_threshold) * 0.25) {
    decision.mode = FrontierScheduleMode::hybrid;
  } else {
    decision.mode = FrontierScheduleMode::vertex_balanced;
  }

  const std::size_t work = decision.mode == FrontierScheduleMode::vertex_balanced
      ? decision.frontier_vertices
      : std::max(decision.frontier_vertices, decision.frontier_edges);
  const std::size_t target_chunks = workers * 8;
  decision.recommended_grain = std::max<std::size_t>(1, (work + target_chunks - 1) / target_chunks);
  return decision;
}

}  // namespace velographx
