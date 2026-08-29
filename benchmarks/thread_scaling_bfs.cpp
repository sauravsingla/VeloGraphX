#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <sched.h>
#endif

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

unsigned available_cpu_count() {
#ifdef __linux__
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) == 0) {
    const auto count = CPU_COUNT(&set);
    if (count > 0) return static_cast<unsigned>(count);
  }
#endif
  return std::max(1u, std::thread::hardware_concurrency());
}

std::vector<unsigned> parse_threads(const std::string& text) {
  std::set<unsigned> unique;
  std::stringstream stream(text);
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (item.empty()) continue;
    const auto value = static_cast<unsigned>(std::stoul(item));
    if (value == 0) throw std::invalid_argument("thread counts must be positive");
    unique.insert(value);
  }
  if (unique.empty()) throw std::invalid_argument("thread list is empty");
  if (!unique.contains(1)) unique.insert(1);
  return {unique.begin(), unique.end()};
}

std::vector<unsigned> default_threads() {
  const auto available = available_cpu_count();
  std::vector<unsigned> out;
  for (unsigned threads = 1; threads <= available && threads <= 64; threads *= 2) {
    out.push_back(threads);
    if (threads > 32) break;
  }
  if (out.back() != available && available <= 64) out.push_back(available);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

struct Measurement {
  unsigned threads{};
  std::int64_t total_ns{};
  double queries_per_second{};
  std::uint64_t digest{};
};

Measurement run_case(const velographx::CsrGraph& graph,
                     const std::vector<velographx::VertexId>& sources,
                     unsigned threads) {
  using clock = std::chrono::steady_clock;
  std::vector<std::uint64_t> digests(sources.size(), 0);
  std::atomic<std::size_t> next{0};
  const auto begin = clock::now();
  std::vector<std::thread> workers;
  workers.reserve(threads);
  for (unsigned t = 0; t < threads; ++t) {
    workers.emplace_back([&] {
      for (;;) {
        const auto i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= sources.size()) break;
        digests[i] = digest(velographx::bfs_distances(graph, sources[i]));
      }
    });
  }
  for (auto& worker : workers) worker.join();
  const auto end = clock::now();
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
  std::uint64_t combined = 0;
  for (const auto value : digests) {
    combined ^= value + 0x9e3779b97f4a7c15ULL + (combined << 6) + (combined >> 2);
  }
  const double qps = ns > 0
                         ? static_cast<double>(sources.size()) * 1e9 /
                               static_cast<double>(ns)
                         : 0.0;
  return {threads, ns, qps, combined};
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) {
    std::cerr << "usage: velographx_thread_scaling_bfs <edge-list> [queries] [threads-csv]\n";
    return 2;
  }

  try {
    const std::filesystem::path path = argv[1];
    const std::size_t queries = argc >= 3 ? static_cast<std::size_t>(std::stoull(argv[2])) : 32;
    auto requested_threads = argc == 4 ? parse_threads(argv[3]) : default_threads();
    const auto available = available_cpu_count();
    requested_threads.erase(
        std::remove_if(requested_threads.begin(), requested_threads.end(),
                       [available](unsigned threads) { return threads > available; }),
        requested_threads.end());
    if (requested_threads.empty()) requested_threads.push_back(1);

    const auto graph = velographx::load_edge_list(path, false);
    if (graph.vertex_count() == 0 || queries == 0) return 2;

    std::vector<velographx::VertexId> sources;
    sources.reserve(queries);
    for (std::size_t i = 0; i < queries; ++i) {
      sources.push_back(static_cast<velographx::VertexId>(
          (i * 2654435761ULL) % graph.vertex_count()));
    }

    std::cout << "threads,queries,total_ns,queries_per_second,speedup,parallel_efficiency,digest\n";
    double baseline_qps = 0.0;
    std::uint64_t baseline_digest = 0;
    for (const auto threads : requested_threads) {
      const auto result = run_case(graph, sources, threads);
      if (baseline_qps == 0.0) {
        baseline_qps = result.queries_per_second;
        baseline_digest = result.digest;
      }
      if (result.digest != baseline_digest) {
        std::cerr << "error: BFS digest differs across thread counts\n";
        return 3;
      }
      const double speedup = baseline_qps > 0.0
                                 ? result.queries_per_second / baseline_qps
                                 : 0.0;
      const double efficiency = speedup / static_cast<double>(threads);
      std::cout << result.threads << ',' << queries << ',' << result.total_ns << ','
                << result.queries_per_second << ',' << speedup << ',' << efficiency
                << ',' << result.digest << '\n';
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
  }
  return 0;
}
