#include <cassert>

#include "velographx/storage/consolidation.hpp"

int main() {
  using namespace velographx;

  ConsolidationController controller;
  ConsolidationSignal quiet{1.05, 1.10, false, false, false};
  ConsolidationSignal latency{1.08, 1.30, false, true, true};
  ConsolidationSignal storage{1.26, 1.05, true, false, true};

  // Five consecutive latency breaches are required before the first cutover.
  assert(!controller.observe(latency, 1));
  assert(!controller.observe(latency, 2));
  assert(!controller.observe(latency, 3));
  assert(!controller.observe(latency, 4));
  assert(controller.observe(latency, 5));
  controller.mark_consolidated(5);
  assert(controller.latency_breach_streak() == 0);

  // Ten-epoch cooldown prevents noisy post-cutover samples from retriggering.
  for (std::size_t epoch = 6; epoch < 15; ++epoch) {
    assert(!controller.observe(latency, epoch));
  }

  // Once cooldown completes, storage remains an immediate safety trigger.
  controller.observe(quiet, 15);
  assert(controller.observe(storage, 15));
  controller.mark_consolidated(15);

  // Intermittent latency noise resets the persistence requirement.
  assert(!controller.observe(latency, 25));
  assert(!controller.observe(quiet, 26));
  assert(!controller.observe(latency, 27));
  assert(!controller.observe(latency, 28));
  assert(!controller.observe(latency, 29));
  assert(!controller.observe(latency, 30));
  assert(controller.observe(latency, 31));

  return 0;
}
