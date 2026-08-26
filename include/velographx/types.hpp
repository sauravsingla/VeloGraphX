#pragma once
#include <cstdint>

namespace velographx {
using VertexId = std::uint32_t;
using EdgeOffset = std::uint64_t;
inline constexpr std::uint32_t kUnreachable = UINT32_MAX;
}
