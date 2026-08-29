#include <cassert>

#include "velographx/storage/consolidation.hpp"

int main() {
  using namespace velographx;

  ConsolidationController controller({5, 3});
  ConsolidationSignal quiet{1.05, 1.10, false, false, false};
  ConsolidationSignal latency{1.08, 1.30, false, true, true};
  ConsolidationSignal storage{1.26, 1.05, true, false, true};

  assert(!controller.observe(latency, 1));
  assert(!controller.observe(latency, 2));
  assert(controller.observe(latency, 3));
  controller.mark_consolidated(3);
  assert(controller.latency_breach_streak() == 0);

  // Cooldown prevents noisy post-cutover latency samples from immediately
  // retriggering maintenance.
  assert(!controller.observe(latency, 4));
  assert(!controller.observe(latency, 5));
  assert(!controller.observe(latency, 6));
  assert(!controller.observe(latency, 7));

  // At the first epoch after cooldown, a storage safety breach can trigger
  // immediately even without a latency streak.
  controller.observe(quiet, 8);
  assert(controller.observe(storage, 8));
  controller.mark_consolidated(8);

  // Intermittent latency noise resets the persistence requirement.
  assert(!controller.observe(latency, 13));
  assert(!controller.observe(quiet, 14));
  assert(!controller.observe(latency, 15));
  assert(!controller.observe(latency, 16));
  assert(controller.observe(latency, 17));

  return 0;
}
