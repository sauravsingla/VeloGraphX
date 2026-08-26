#include "velographx/runtime/numa_policy.hpp"

#include <cassert>
#include <cstdint>

int main() {
  velographx::NumaInfo info;
  info.hardware_threads = 8;
  info.nodes = 2;
  info.native_support = true;
  info.topology = {{0, {0, 1, 2, 3}}, {1, {4, 5, 6, 7}}};

  auto off = velographx::choose_numa_placement(info, velographx::NumaMode::off, 3);
  assert(!off.node_id.has_value());
  assert(!off.cpu_id.has_value());

  auto p0 = velographx::choose_numa_placement(info, velographx::NumaMode::auto_detect, 0);
  auto p1 = velographx::choose_numa_placement(info, velographx::NumaMode::auto_detect, 1);
  auto p2 = velographx::choose_numa_placement(info, velographx::NumaMode::auto_detect, 2);
  assert(p0.node_id == 0 && p0.cpu_id == 0);
  assert(p1.node_id == 1 && p1.cpu_id == 4);
  assert(p2.node_id == 0 && p2.cpu_id == 1);

  auto placements = velographx::plan_numa_workers(info, velographx::NumaMode::interleave, 8);
  assert(placements.size() == 8);
  assert(placements[7].node_id == 1);
  assert(placements[7].cpu_id == 7);

  const auto description = velographx::describe_numa_placement(placements[0]);
  assert(description.find("mode=interleave") != std::string::npos);
  assert(description.find("node=0") != std::string::npos);

#if defined(__linux__)
  auto region = velographx::allocate_numa_memory(8192, info, velographx::NumaMode::off);
  assert(region.data != nullptr);
  assert(region.bytes == 8192);
  assert(!region.native_policy_applied);
  velographx::first_touch_region(region);
  auto* data = static_cast<std::uint8_t*>(region.data);
  data[0] = 7;
  data[4096] = 9;
  assert(data[0] == 7 && data[4096] == 9);
  velographx::release_numa_memory(region);
  assert(region.data == nullptr && region.bytes == 0);

  auto empty = velographx::allocate_numa_memory(0, info, velographx::NumaMode::off);
  assert(empty.data == nullptr && empty.bytes == 0);
#else
  auto region = velographx::allocate_numa_memory(8192, info, velographx::NumaMode::off);
  assert(region.data == nullptr);
#endif
  return 0;
}
