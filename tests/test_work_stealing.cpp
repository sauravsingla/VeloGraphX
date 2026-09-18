#include "velographx/runtime/work_stealing_pool.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
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

  // Force a real stealing opportunity instead of relying on scheduler timing.
  // All tasks start on queue 0 and remain blocked until at least one worker has
  // attempted to steal from another queue.
  std::atomic<bool> release_probe{false};
  for (std::size_t i = 0; i < 32; ++i) {
    pool.submit([&release_probe] {
      while (!release_probe.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
    }, 0);
  }

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (pool.stats().steal_attempts == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  assert(pool.stats().steal_attempts > 0);
  release_probe.store(true, std::memory_order_release);
  pool.wait_idle();

  std::atomic<std::size_t> sum{0};
  for (std::size_t i = 0; i < 1000; ++i) {
    pool.submit([&sum, i] { sum.fetch_add(i, std::memory_order_relaxed); }, 0);
  }
  pool.wait_idle();
  assert(sum.load(std::memory_order_relaxed) == 999 * 1000 / 2);

  std::vector<std::size_t> values(2048, 0);
  pool.parallel_for(0, values.size(), [&values](std::size_t i) { values[i] = i + 1; });
  for (std::size_t i = 0; i < values.size(); ++i) assert(values[i] == i + 1);

  // Task failures are reported to the caller only after all outstanding work
  // is accounted for. A failed task must not terminate a worker or poison the
  // pool for later submissions.
  pool.submit([] { throw std::runtime_error("expected task failure"); }, 0);
  bool saw_task_failure = false;
  try {
    pool.wait_idle();
  } catch (const std::runtime_error& error) {
    saw_task_failure = std::string(error.what()) == "expected task failure";
  }
  assert(saw_task_failure);

  std::atomic<bool> ran_after_failure{false};
  pool.submit([&ran_after_failure] {
    ran_after_failure.store(true, std::memory_order_release);
  }, 0);
  pool.wait_idle();
  assert(ran_after_failure.load(std::memory_order_acquire));

  // Destruction is intentionally non-reporting: callers that need task
  // exceptions must call wait_idle()/parallel_for() explicitly.
  {
    WorkStealingPool destructor_pool(2);
    destructor_pool.submit([] { throw std::runtime_error("unobserved task failure"); });
  }

  const auto stats = pool.stats();
  assert(stats.submitted > 1000);
  assert(stats.executed == stats.submitted);
  assert(stats.steal_attempts > 0);
  assert(stats.local_steals + stats.remote_steals == stats.successful_steals);
  return 0;
}
