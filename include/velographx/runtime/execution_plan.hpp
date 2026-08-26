#pragma once
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
};

struct ExecutionPlan {
  ExecutionMode mode{ExecutionMode::full_recompute};
  double incremental_cost{0};
  double full_cost{0};
  std::string reason;
};

inline ExecutionPlan choose_execution(const ExecutionEstimate& e) {
  const double inc = static_cast<double>(e.changed_edges) * (1.0 + e.frontier_growth) + static_cast<double>(e.affected_vertices);
  const double full = static_cast<double>(e.total_edges) + static_cast<double>(e.total_vertices);
  if (inc < 0.65 * full) return {ExecutionMode::incremental, inc, full, "estimated affected-region work is lower than full traversal"};
  return {ExecutionMode::full_recompute, inc, full, "propagation estimate approaches graph-scale work"};
}

inline std::string explain(const ExecutionPlan& p) {
  return std::string("mode=") + (p.mode == ExecutionMode::incremental ? "incremental" : "full_recompute") +
         "; incremental_cost=" + std::to_string(p.incremental_cost) +
         "; full_cost=" + std::to_string(p.full_cost) + "; reason=" + p.reason;
}
} // namespace velographx
