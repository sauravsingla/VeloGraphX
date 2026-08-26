#include "velographx/runtime/degree_frontier_scheduler.hpp"

#include <cassert>
#include <vector>

int main() {
  using velographx::FrontierScheduleMode;
  using velographx::choose_frontier_schedule;

  {
    std::vector<std::size_t> degrees(128, 2);
    const auto decision = choose_frontier_schedule(degrees, 8);
    assert(decision.mode == FrontierScheduleMode::vertex_balanced);
    assert(decision.frontier_vertices == 128);
    assert(decision.frontier_edges == 256);
    assert(decision.recommended_grain >= 1);
  }

  {
    std::vector<std::size_t> degrees(64, 24);
    const auto decision = choose_frontier_schedule(degrees, 4);
    assert(decision.mode == FrontierScheduleMode::hybrid);
    assert(decision.average_degree == 24.0);
  }

  {
    std::vector<std::size_t> degrees(32, 4);
    for (std::size_t i = 0; i < 8; ++i) degrees[i] = 128;
    const auto decision = choose_frontier_schedule(degrees, 4);
    assert(decision.mode == FrontierScheduleMode::edge_balanced);
    assert(decision.frontier_edges > decision.frontier_vertices);
  }

  {
    const auto decision = choose_frontier_schedule({}, 0);
    assert(decision.mode == FrontierScheduleMode::vertex_balanced);
    assert(decision.recommended_grain == 1);
  }

  return 0;
}
