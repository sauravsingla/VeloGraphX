#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace velographx {
struct RuntimeMetrics { std::size_t vertices{0}; std::size_t edges{0}; std::size_t changed_edges{0}; std::size_t affected_vertices{0}; std::size_t frontier_iterations{0}; std::size_t threads{1}; std::uint64_t elapsed_us{0}; std::string kernel{"scalar"}; std::string mode{"full_recompute"}; };
}
