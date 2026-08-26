#pragma once

#include "velographx/runtime/numa.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#if __has_include(<linux/mempolicy.h>)
#include <linux/mempolicy.h>
#endif
#endif

namespace velographx {

struct NumaPlacement {
  NumaMode mode{NumaMode::auto_detect};
  std::size_t worker_index{0};
  std::optional<std::size_t> node_id;
  std::optional<std::size_t> cpu_id;
};

struct NumaMemoryRegion {
  void* data{nullptr};
  std::size_t bytes{0};
  NumaMode mode{NumaMode::off};
  std::optional<std::size_t> node_id;
  bool native_policy_applied{false};
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
  if (cpu_id >= CPU_SETSIZE) return false;
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

inline bool linux_mbind_region(void* address, std::size_t bytes, NumaMode mode,
                               std::optional<std::size_t> node_id,
                               std::size_t max_node_id) noexcept {
#if defined(__linux__) && defined(SYS_mbind) && defined(MPOL_BIND) && defined(MPOL_INTERLEAVE)
  if (address == nullptr || bytes == 0 || mode == NumaMode::off) return mode == NumaMode::off;
  const std::size_t word_bits = sizeof(unsigned long) * 8U;
  const std::size_t maxnode = max_node_id + 1U;
  std::vector<unsigned long> mask((maxnode + word_bits - 1U) / word_bits, 0UL);
  if (mode == NumaMode::interleave) {
    for (std::size_t node = 0; node <= max_node_id; ++node)
      mask[node / word_bits] |= 1UL << (node % word_bits);
  } else {
    if (!node_id.has_value() || *node_id > max_node_id) return false;
    mask[*node_id / word_bits] |= 1UL << (*node_id % word_bits);
  }
  const int policy = mode == NumaMode::interleave ? MPOL_INTERLEAVE : MPOL_BIND;
  return ::syscall(SYS_mbind, address, bytes, policy, mask.data(), maxnode, 0UL) == 0;
#else
  (void)address; (void)bytes; (void)mode; (void)node_id; (void)max_node_id;
  return false;
#endif
}

inline NumaMemoryRegion allocate_numa_memory(std::size_t bytes, const NumaInfo& info,
                                             NumaMode mode,
                                             std::optional<std::size_t> node_id = std::nullopt) noexcept {
  NumaMemoryRegion region{nullptr, bytes, mode, node_id, false};
  if (bytes == 0) return region;
#if defined(__linux__)
  void* memory = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED) return region;
  region.data = memory;
  if (mode != NumaMode::off && info.native_support && !info.topology.empty()) {
    std::size_t max_node_id = 0;
    for (const auto& node : info.topology) if (node.id > max_node_id) max_node_id = node.id;
    region.native_policy_applied = linux_mbind_region(memory, bytes, mode, node_id, max_node_id);
  }
#else
  (void)info;
#endif
  return region;
}

inline void first_touch_region(NumaMemoryRegion& region, std::size_t stride = 4096) noexcept {
  if (region.data == nullptr || region.bytes == 0) return;
  if (stride == 0) stride = 4096;
  auto* bytes = static_cast<volatile std::uint8_t*>(region.data);
  for (std::size_t offset = 0; offset < region.bytes; offset += stride) bytes[offset] = 0;
  bytes[region.bytes - 1] = 0;
}

inline void release_numa_memory(NumaMemoryRegion& region) noexcept {
#if defined(__linux__)
  if (region.data != nullptr && region.bytes != 0) ::munmap(region.data, region.bytes);
#endif
  region.data = nullptr;
  region.bytes = 0;
  region.native_policy_applied = false;
}

inline std::string describe_numa_placement(const NumaPlacement& placement) {
  std::string out = "mode=" + numa_mode_name(placement.mode) +
                    " worker=" + std::to_string(placement.worker_index);
  out += placement.node_id ? " node=" + std::to_string(*placement.node_id) : " node=none";
  out += placement.cpu_id ? " cpu=" + std::to_string(*placement.cpu_id) : " cpu=none";
  return out;
}

}  // namespace velographx
