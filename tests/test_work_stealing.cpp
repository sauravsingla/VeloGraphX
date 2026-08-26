#include "velographx/runtime/work_stealing_pool.hpp"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>

int main() {
  using velographx::WorkStealingPool;

  assert(WorkStealingPool::adaptive_grain(0, 4) == 1);
  assert(WorkStealingPool::adaptive_grain(1000, 4) >= 1);

  WorkStealingPool pool(4, {0, 1, 0, 1});
  assert(pool.queue_group(0) == 0);
  assert(pool.queue_group(1) == 1);
  assert(pool.queue_group(2) == 0);
  assert(pool.queue_group(3) == 1);

  std::atomic<std::size_t> sum{0};
  for (std::size_t i = 0; i < 1000; ++i) {
    pool.submit([&sum, i] { sum.fetch_add(i, std::memory_order_relaxed); }, 0);
  }
  pool.wait_idle();
  assert(sum.load(std::memory_order_relaxed) == 999 * 1000 / 2);

  std::vector<std::size_t> values(2048, 0);
  pool.parallel_for(0, values.size(), [&values](std::size_t i) { values[i] = i + 1; });
  for (std::size_t i = 0; i < values.size(); ++i) assert(values[i] == i + 1);

  const auto stats = pool.stats();
  assert(stats.submitted > 1000);
  assert(stats.executed == stats.submitted);
  assert(stats.steal_attempts > 0);
  assert(stats.local_steals + stats.remote_steals == stats.successful_steals);
  return 0;
}
