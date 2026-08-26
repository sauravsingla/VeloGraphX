#pragma once
#include <algorithm>
#include <cstddef>
#include <string>

namespace velographx {

enum class ExecutionMode { incremental, full_recompute };

struct ExecutionEstimate {
  std::size_t changed_edges{0};
  std::size_t total_edges{0};
  std::size_t affected_vertices{0};
  std::size_t total_vertices{0};
  double frontier_growth{1.0};
  double observed_affected_edge_fraction{0.0};
  double historical_incremental_speedup{1.0};
  double historical_repair_success_rate{1.0};
};

struct ExecutionPlan {
  ExecutionMode mode{ExecutionMode::full_recompute};
  double incremental_cost{0};
  double full_cost{0};
  double estimated_work_fraction{1.0};
  double confidence{0.0};
  std::string reason;
};

inline ExecutionPlan choose_execution(const ExecutionEstimate& e) {
  const double changed_fraction = e.total_edges == 0
                                      ? 0.0
                                      : static_cast<double>(e.changed_edges) /
                                            static_cast<double>(e.total_edges);
  const double affected_vertex_fraction = e.total_vertices == 0
                                              ? 0.0
                                              : static_cast<double>(e.affected_vertices) /
                                                    static_cast<double>(e.total_vertices);
  const double observed_fraction = std::clamp(e.observed_affected_edge_fraction, 0.0, 1.0);
  const double propagation = std::max(1.0, e.frontier_growth);

  const double structural_fraction = std::clamp(
      0.45 * changed_fraction * propagation +
          0.35 * affected_vertex_fraction +
          0.20 * observed_fraction,
      0.0, 1.5);

  const double speedup = std::max(0.1, e.historical_incremental_speedup);
  const double repair_success = std::clamp(e.historical_repair_success_rate, 0.0, 1.0);
  const double history_penalty = (1.0 / speedup) * (1.0 + (1.0 - repair_success));

  const double full = static_cast<double>(e.total_edges) +
                      static_cast<double>(e.total_vertices);
  const double inc = full * structural_fraction * history_penalty;
  const double work_fraction = full == 0.0 ? 0.0 : inc / full;
  const double confidence = std::clamp(
      0.5 * repair_success +
          0.3 * std::min(1.0, speedup / 2.0) +
          0.2 * (1.0 - std::min(1.0, structural_fraction)),
      0.0, 1.0);

  if (inc < 0.65 * full && repair_success >= 0.5) {
    return {ExecutionMode::incremental, inc, full, work_fraction, confidence,
            "affected-work estimate and historical repair behavior favor incremental execution"};
  }
  return {ExecutionMode::full_recompute, inc, full, work_fraction, confidence,
          "estimated propagation or historical repair risk favors full recomputation"};
}

inline std::string explain(const ExecutionPlan& p) {
  return std::string("mode=") +
         (p.mode == ExecutionMode::incremental ? "incremental" : "full_recompute") +
         "; incremental_cost=" + std::to_string(p.incremental_cost) +
         "; full_cost=" + std::to_string(p.full_cost) +
         "; estimated_work_fraction=" + std::to_string(p.estimated_work_fraction) +
         "; confidence=" + std::to_string(p.confidence) +
         "; reason=" + p.reason;
}
} // namespace velographx
