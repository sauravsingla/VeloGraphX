#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include "velographx/storage/compressed_adjacency.hpp"
#include "velographx/storage/compressed_decode_simd.hpp"

namespace {
using Clock = std::chrono::steady_clock;
using VertexId = velographx::storage::VertexId;

std::vector<std::vector<VertexId>> make_graph(std::size_t vertices, std::size_t degree) {
  std::vector<std::vector<VertexId>> rows(vertices);
  for (std::size_t u = 0; u < vertices; ++u) {
    auto& row = rows[u];
    row.reserve(degree * 2);
    for (std::size_t d = 1; d <= degree; ++d) {
      row.push_back(static_cast<VertexId>((u + d) % vertices));
      row.push_back(static_cast<VertexId>((u + vertices - d) % vertices));
    }
    std::sort(row.begin(), row.end());
    row.erase(std::unique(row.begin(), row.end()), row.end());
  }
  return rows;
}

template <class NeighborProvider>
std::uint64_t bfs_digest(std::size_t vertices, NeighborProvider&& neighbors) {
  constexpr auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> dist(vertices, unreachable);
  std::queue<VertexId> q;
  dist[0] = 0;
  q.push(0);
  while (!q.empty()) {
    const auto u = q.front(); q.pop();
    const auto row = neighbors(u);
    for (auto v : row) {
      if (dist[v] != unreachable) continue;
      dist[v] = dist[u] + 1;
      q.push(v);
    }
  }
  std::uint64_t h = 1469598103934665603ULL;
  for (auto d : dist) { h ^= d; h *= 1099511628211ULL; }
  return h;
}

template <class F>
double median_ms(F&& f, std::uint64_t expected_digest, int repeats = 5) {
  std::vector<double> values;
  for (int i = 0; i < repeats; ++i) {
    const auto t0 = Clock::now();
    const auto digest = f();
    const auto t1 = Clock::now();
    if (digest != expected_digest) throw std::runtime_error("compressed traversal correctness mismatch");
    values.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}
}  // namespace

int main(int argc, char** argv) {
  using namespace velographx::storage;
  const std::size_t vertices = argc > 1 ? std::stoull(argv[1]) : 100000;
  const std::size_t degree = argc > 2 ? std::stoull(argv[2]) : 8;
  const auto rows = make_graph(vertices, degree);

  std::size_t raw_bytes = 0;
  for (const auto& row : rows) raw_bytes += row.size() * sizeof(VertexId);
  const auto raw_digest = bfs_digest(vertices, [&](VertexId u) { return rows[u]; });
  const auto raw_ms = median_ms([&] { return bfs_digest(vertices, [&](VertexId u) { return rows[u]; }); }, raw_digest);

  std::vector<std::vector<std::uint8_t>> varbyte(vertices);
  std::vector<BlockedAdjacency> blocked(vertices);
  std::vector<SimdFriendlyAdjacency> fixed(vertices);
  std::size_t varbyte_bytes = 0, blocked_bytes = 0, fixed_bytes = 0;
  for (std::size_t u = 0; u < vertices; ++u) {
    varbyte[u] = variable_byte_delta_encode(rows[u]);
    blocked[u] = blocked_variable_byte_encode(rows[u], 128);
    fixed[u] = simd_friendly_delta_encode(rows[u], 128);
    varbyte_bytes += varbyte[u].size();
    blocked_bytes += blocked[u].payload.size() + blocked[u].block_offsets.size() * sizeof(std::size_t);
    fixed_bytes += fixed[u].payload.size() + fixed[u].blocks.size() * sizeof(FixedWidthDeltaBlock);
  }

  const auto varbyte_ms = median_ms([&] {
    return bfs_digest(vertices, [&](VertexId u) { return variable_byte_delta_decode(varbyte[u]); });
  }, raw_digest);
  const auto blocked_ms = median_ms([&] {
    return bfs_digest(vertices, [&](VertexId u) { return blocked_variable_byte_decode(blocked[u]); });
  }, raw_digest);
  const auto fixed_scalar_ms = median_ms([&] {
    return bfs_digest(vertices, [&](VertexId u) { return simd_friendly_delta_decode(fixed[u]); });
  }, raw_digest);
  const auto fixed_vector_ms = median_ms([&] {
    return bfs_digest(vertices, [&](VertexId u) { return simd_friendly_delta_decode_vectorized(fixed[u]); });
  }, raw_digest);

  auto emit = [&](const char* codec, std::size_t bytes, double ms) {
    const double ratio = bytes ? static_cast<double>(raw_bytes) / static_cast<double>(bytes) : 0.0;
    const double traversal_ratio = raw_ms > 0.0 ? ms / raw_ms : 0.0;
    std::cout << codec << ',' << raw_bytes << ',' << bytes << ',' << ratio << ','
              << raw_ms << ',' << ms << ',' << traversal_ratio << ',' << raw_digest << '\n';
  };
  std::cout << "codec,raw_bytes,encoded_bytes,compression_ratio,raw_bfs_ms,codec_bfs_ms,relative_to_raw,digest\n";
  emit("varbyte", varbyte_bytes, varbyte_ms);
  emit("blocked-varbyte", blocked_bytes, blocked_ms);
  emit("fixed-width-scalar", fixed_bytes, fixed_scalar_ms);
  emit("fixed-width-vectorized", fixed_bytes, fixed_vector_ms);
  return 0;
}
