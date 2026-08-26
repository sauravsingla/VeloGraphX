#include "velographx/runtime/work_stealing_pool.hpp"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <thread>
#include <vector>

int main() {
  using velographx::WorkStealingPool;

  constexpr std::size_t producers = 4;
  constexpr std::size_t rounds = 20;
  constexpr std::size_t tasks_per_producer = 250;

  WorkStealingPool pool(4);
  std::atomic<std::size_t> executed{0};

  for (std::size_t round = 0; round < rounds; ++round) {
    std::vector<std::thread> submitters;
    submitters.reserve(producers);
    for (std::size_t producer = 0; producer < producers; ++producer) {
      submitters.emplace_back([&pool, &executed, producer] {
        for (std::size_t i = 0; i < tasks_per_producer; ++i) {
          pool.submit([&executed] {
            executed.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
          }, producer);
        }
      });
    }
    for (auto& submitter : submitters) submitter.join();
    pool.wait_idle();

    const auto expected = (round + 1) * producers * tasks_per_producer;
    assert(executed.load(std::memory_order_relaxed) == expected);
    const auto stats = pool.stats();
    assert(stats.submitted == expected);
    assert(stats.executed == expected);
  }

  return 0;
}
