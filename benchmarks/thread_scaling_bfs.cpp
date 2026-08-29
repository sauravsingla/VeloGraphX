#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

#include "velographx/algorithms.hpp"
#include "velographx/io.hpp"

namespace {
std::uint64_t digest(const std::vector<std::uint32_t>& distances) {
  std::uint64_t h = 1469598103934665603ULL;
  for (std::uint32_t v : distances) {
    h ^= static_cast<std::uint64_t>(v);
    h *= 1099511628211ULL;
  }
  return h;
}
}

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: velographx_thread_scaling_bfs <edge-list> [queries]\n";
    return 2;
  }
  const std::filesystem::path path = argv[1];
  const std::size_t queries = argc == 3 ? static_cast<std::size_t>(std::stoull(argv[2])) : 12;
  const auto graph = velographx::load_edge_list(path, false);
  if (graph.vertex_count() == 0 || queries == 0) return 2;

  std::vector<velographx::VertexId> sources;
  sources.reserve(queries);
  for (std::size_t i = 0; i < queries; ++i) {
    sources.push_back(static_cast<velographx::VertexId>((i * 2654435761ULL) % graph.vertex_count()));
  }

  using clock = std::chrono::steady_clock;
  std::cout << "threads,queries,total_ns,queries_per_second,digest\n";
  for (unsigned threads : {1u, 2u, 4u}) {
    std::vector<std::uint64_t> digests(queries, 0);
    std::atomic<std::size_t> next{0};
    const auto begin = clock::now();
    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (unsigned t = 0; t < threads; ++t) {
      workers.emplace_back([&] {
        for (;;) {
          const auto i = next.fetch_add(1, std::memory_order_relaxed);
          if (i >= queries) break;
          digests[i] = digest(velographx::bfs_distances(graph, sources[i]));
        }
      });
    }
    for (auto& worker : workers) worker.join();
    const auto end = clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
    std::uint64_t combined = 0;
    for (std::size_t i = 0; i < digests.size(); ++i) combined ^= digests[i] + 0x9e3779b97f4a7c15ULL + (combined << 6) + (combined >> 2);
    const double qps = ns > 0 ? static_cast<double>(queries) * 1e9 / static_cast<double>(ns) : 0.0;
    std::cout << threads << ',' << queries << ',' << ns << ',' << qps << ',' << combined << '\n';
  }
  return 0;
}
