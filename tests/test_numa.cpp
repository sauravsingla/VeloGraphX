#include <cassert>
#include <vector>

#include "velographx/runtime/numa.hpp"

int main() {
  const auto parsed = velographx::parse_cpu_list("0-3,8,10-11");
  const std::vector<std::size_t> expected{0, 1, 2, 3, 8, 10, 11};
  assert(parsed == expected);

  assert(velographx::parse_cpu_list("4").size() == 1);
  assert(velographx::parse_cpu_list("").empty());
  assert(velographx::numa_mode_name(velographx::NumaMode::off) == "off");
  assert(velographx::numa_mode_name(velographx::NumaMode::interleave) == "interleave");
  assert(velographx::numa_mode_name(velographx::NumaMode::auto_detect) == "auto");

  const auto info = velographx::detect_numa();
  assert(info.nodes >= 1);
  if (info.native_support) assert(!info.topology.empty());
}
