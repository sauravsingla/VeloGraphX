#include "velographx/runtime/numa_scheduler.hpp"

#include <atomic>
#include <cassert>
#include <vector>

int main() {
  velographx::NumaInfo info;
  info.hardware_threads = 4;
  info.nodes = 2;
  info.native_support = true;
  info.topology = {{0, {0, 2}}, {1, {1, 3}}};

  auto plan = velographx::plan_numa_partitions(100, info, velographx::NumaMode::auto_detect, 4);
  velographx::WorkStealingPool pool(4);
  velographx::NumaLocalScheduler scheduler(pool, plan);

  assert(scheduler.preferred_queue_for_vertex(0, 0) == 0);
  assert(scheduler.preferred_queue_for_vertex(25, 0) == 1);
  assert(scheduler.preferred_queue_for_vertex(50, 1) == 2);
  assert(scheduler.preferred_queue_for_vertex(75, 1) == 3);

  std::atomic<std::size_t> visited{0};
  scheduler.parallel_for_partitions([&](const velographx::NumaGraphPartition& partition) {
    visited.fetch_add(partition.end_vertex - partition.begin_vertex,
                      std::memory_order_relaxed);
  });
  assert(visited.load(std::memory_order_relaxed) == 100);

  std::atomic<int> marker{0};
  scheduler.submit_for_vertex(80, [&] { marker.store(1, std::memory_order_relaxed); });
  pool.wait_idle();
  assert(marker.load(std::memory_order_relaxed) == 1);
  return 0;
}
