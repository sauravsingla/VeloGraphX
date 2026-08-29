#include <cassert>

#include "velographx/storage/consolidation.hpp"

int main() {
  using namespace velographx;

  ConsolidationController controller;
  ConsolidationSignal quiet{1.05, 1.10, false, false, false};
  ConsolidationSignal latency_noise{1.08, 1.30, false, true, true};
  ConsolidationSignal latency_with_growth{1.12, 1.30, false, true, true};
  ConsolidationSignal storage{1.26, 1.05, true, false, true};

  // Latency alone is insufficient when compact-patch growth is still small.
  for (std::size_t epoch = 1; epoch <= 8; ++epoch) {
    assert(!controller.observe(latency_noise, epoch));
  }
  assert(controller.latency_breach_streak() == 0);

  // Five consecutive latency breaches with meaningful patch growth trigger.
  assert(!controller.observe(latency_with_growth, 9));
  assert(!controller.observe(latency_with_growth, 10));
  assert(!controller.observe(latency_with_growth, 11));
  assert(!controller.observe(latency_with_growth, 12));
  assert(controller.observe(latency_with_growth, 13));
  controller.mark_consolidated(13);
  assert(controller.latency_breach_streak() == 0);

  // The 1.25x storage safety limit bypasses latency cooldown immediately.
  assert(controller.observe(storage, 14));
  controller.mark_consolidated(14);

  // Latency-driven work still observes the ten-epoch cooldown.
  for (std::size_t epoch = 15; epoch < 24; ++epoch) {
    assert(!controller.observe(latency_with_growth, epoch));
  }

  // Intermittent latency or low patch growth resets persistence.
  assert(!controller.observe(latency_with_growth, 24));
  assert(!controller.observe(latency_with_growth, 25));
  assert(!controller.observe(quiet, 26));
  assert(!controller.observe(latency_with_growth, 27));
  assert(!controller.observe(latency_with_growth, 28));
  assert(!controller.observe(latency_with_growth, 29));
  assert(!controller.observe(latency_with_growth, 30));
  assert(controller.observe(latency_with_growth, 31));

  return 0;
}
