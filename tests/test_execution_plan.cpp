#include "velographx/runtime/execution_plan.hpp"

#include <cassert>
#include <string>

int main() {
  using velographx::ExecutionEstimate;
  using velographx::ExecutionMode;
  using velographx::choose_execution;
  using velographx::explain;

  {
    ExecutionEstimate estimate;
    estimate.changed_edges = 10;
    estimate.total_edges = 100000;
    estimate.affected_vertices = 20;
    estimate.total_vertices = 10000;
    estimate.frontier_growth = 1.2;
    estimate.observed_affected_edge_fraction = 0.001;
    estimate.historical_incremental_speedup = 3.0;
    estimate.historical_repair_success_rate = 0.95;

    const auto plan = choose_execution(estimate);
    assert(plan.mode == ExecutionMode::incremental);
    assert(plan.incremental_cost < plan.full_cost);
    assert(plan.estimated_work_fraction < 0.65);
    assert(plan.confidence > 0.5);
    const auto text = explain(plan);
    assert(text.find("estimated_work_fraction=") != std::string::npos);
    assert(text.find("confidence=") != std::string::npos);
  }

  {
    ExecutionEstimate estimate;
    estimate.changed_edges = 10000;
    estimate.total_edges = 100000;
    estimate.affected_vertices = 5000;
    estimate.total_vertices = 10000;
    estimate.frontier_growth = 4.0;
    estimate.observed_affected_edge_fraction = 0.7;
    estimate.historical_incremental_speedup = 0.8;
    estimate.historical_repair_success_rate = 0.4;

    const auto plan = choose_execution(estimate);
    assert(plan.mode == ExecutionMode::full_recompute);
    assert(plan.full_cost > 0.0);
    assert(plan.reason.find("full recomputation") != std::string::npos);
  }

  {
    ExecutionEstimate estimate;
    const auto plan = choose_execution(estimate);
    assert(plan.full_cost == 0.0);
    assert(plan.estimated_work_fraction == 0.0);
  }

  return 0;
}
