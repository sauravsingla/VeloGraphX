#include "velographx/runtime/partitioner.hpp"

#include <cassert>

int main() {
  velographx::NumaInfo info;
  info.hardware_threads = 8;
  info.nodes = 2;
  info.native_support = true;
  info.topology = {{0, {0, 1, 2, 3}}, {1, {4, 5, 6, 7}}};

  const auto placements = velographx::plan_numa_vertex_partitions(
      100, info, velographx::NumaMode::auto_detect, 4);
  assert(placements.size() == 4);
  assert(placements[0].vertex_begin == 0 && placements[0].vertex_end == 25);
  assert(placements[1].vertex_begin == 25 && placements[1].vertex_end == 50);
  assert(placements[2].vertex_begin == 50 && placements[2].vertex_end == 75);
  assert(placements[3].vertex_begin == 75 && placements[3].vertex_end == 100);
  assert(placements[0].node_id == 0);
  assert(placements[1].node_id == 1);
  assert(placements[2].node_id == 0);
  assert(placements[3].node_id == 1);
  assert(placements[0].local_cpus.size() == 4);
  assert(placements[1].local_cpus.front() == 4);

  assert(velographx::numa_node_for_vertex(0, placements) == 0);
  assert(velographx::numa_node_for_vertex(49, placements) == 1);
  assert(velographx::numa_node_for_vertex(74, placements) == 0);
  assert(velographx::numa_node_for_vertex(99, placements) == 1);
  assert(!velographx::numa_node_for_vertex(100, placements).has_value());

  const auto off = velographx::plan_numa_vertex_partitions(
      10, info, velographx::NumaMode::off, 2);
  assert(off.size() == 2);
  assert(!off[0].node_id.has_value());
  assert(off[0].local_cpus.empty());

  // Default partition count follows NUMA topology when available.
  const auto automatic = velographx::plan_numa_vertex_partitions(
      8, info, velographx::NumaMode::interleave);
  assert(automatic.size() == 2);
  assert(automatic[0].vertex_begin == 0 && automatic[0].vertex_end == 4);
  assert(automatic[1].vertex_begin == 4 && automatic[1].vertex_end == 8);
  return 0;
}
