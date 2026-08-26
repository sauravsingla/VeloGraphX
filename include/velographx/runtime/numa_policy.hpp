#pragma once

#include "velographx/runtime/numa.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace velographx {

struct NumaPlacement {
  NumaMode mode{NumaMode::auto_detect};
  std::size_t worker_index{0};
  std::optional<std::size_t> node_id;
  std::optional<std::size_t> cpu_id;
};

inline NumaPlacement choose_numa_placement(const NumaInfo& info, NumaMode mode,
                                           std::size_t worker_index) {
  NumaPlacement placement{mode, worker_index, std::nullopt, std::nullopt};
  if (mode == NumaMode::off || info.topology.empty()) return placement;

  const auto node_index = worker_index % info.topology.size();
  const auto& node = info.topology[node_index];
  placement.node_id = node.id;
  if (!node.cpus.empty()) {
    const auto local_worker = worker_index / info.topology.size();
    placement.cpu_id = node.cpus[local_worker % node.cpus.size()];
  }
  return placement;
}

inline bool pin_current_thread_to_cpu(std::size_t cpu_id) noexcept {
#if defined(__linux__)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);
  return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
  (void)cpu_id;
  return false;
#endif
}

inline bool apply_numa_placement(const NumaPlacement& placement) noexcept {
  if (!placement.cpu_id.has_value()) return placement.mode == NumaMode::off;
  return pin_current_thread_to_cpu(*placement.cpu_id);
}

inline std::vector<NumaPlacement> plan_numa_workers(const NumaInfo& info, NumaMode mode,
                                                    std::size_t workers) {
  std::vector<NumaPlacement> placements;
  placements.reserve(workers);
  for (std::size_t i = 0; i < workers; ++i) {
    placements.push_back(choose_numa_placement(info, mode, i));
  }
  return placements;
}

inline std::string describe_numa_placement(const NumaPlacement& placement) {
  std::string out = "mode=" + numa_mode_name(placement.mode) +
                    " worker=" + std::to_string(placement.worker_index);
  out += placement.node_id ? " node=" + std::to_string(*placement.node_id) : " node=none";
  out += placement.cpu_id ? " cpu=" + std::to_string(*placement.cpu_id) : " cpu=none";
  return out;
}

}  // namespace velographx
