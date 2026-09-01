#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "velographx/algorithms.hpp"
#include "velographx/csr_graph.hpp"

namespace {
using Clock = std::chrono::steady_clock;
using velographx::CsrGraph;
using velographx::VertexId;

std::vector<std::pair<VertexId, VertexId>> make_edges(std::size_t vertices, std::size_t degree) {
  std::vector<std::pair<VertexId, VertexId>> edges;
  edges.reserve(vertices * degree);
  for (std::size_t u = 0; u < vertices; ++u) {
    for (std::size_t d = 1; d <= degree; ++d) {
      const auto v = static_cast<VertexId>((u + d * 7919ULL) % vertices);
      if (u != v) edges.emplace_back(static_cast<VertexId>(u), v);
    }
  }
  return edges;
}

std::uint64_t hash_u32(const std::vector<std::uint32_t>& values) {
  std::uint64_t h = 1469598103934665603ULL;
  for (auto v : values) { h ^= v; h *= 1099511628211ULL; }
  return h;
}
std::uint64_t hash_vid(const std::vector<VertexId>& values) {
  std::uint64_t h = 1469598103934665603ULL;
  for (auto v : values) { h ^= v; h *= 1099511628211ULL; }
  return h;
}

std::vector<unsigned> parse_threads(const std::string& text) {
  std::set<unsigned> unique;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) if (!item.empty()) unique.insert(static_cast<unsigned>(std::stoul(item)));
  if (!unique.count(1)) unique.insert(1);
  return {unique.begin(), unique.end()};
}

template <class Task>
std::pair<double, std::uint64_t> run_parallel(std::size_t tasks, unsigned threads, Task&& task) {
  std::atomic<std::size_t> next{0};
  std::vector<std::uint64_t> digests(tasks);
  std::vector<std::thread> workers;
  const auto t0 = Clock::now();
  for (unsigned t = 0; t < threads; ++t) {
    workers.emplace_back([&] {
      while (true) {
        const auto i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= tasks) break;
        digests[i] = task(i);
      }
    });
  }
  for (auto& w : workers) w.join();
  const auto t1 = Clock::now();
  std::uint64_t combined = 0;
  for (auto h : digests) combined ^= h + 0x9e3779b97f4a7c15ULL + (combined << 6) + (combined >> 2);
  const double sec = std::chrono::duration<double>(t1 - t0).count();
  return {sec > 0 ? static_cast<double>(tasks) / sec : 0.0, combined};
}

template <class Task>
void emit_algorithm(const char* name, std::size_t tasks, const std::vector<unsigned>& threads, Task&& task) {
  double baseline = 0.0;
  std::uint64_t expected = 0;
  for (auto t : threads) {
    const auto [qps, digest] = run_parallel(tasks, t, task);
    if (baseline == 0.0) { baseline = qps; expected = digest; }
    if (digest != expected) throw std::runtime_error(std::string(name) + " digest mismatch across threads");
    const double speedup = baseline > 0 ? qps / baseline : 0.0;
    std::cout << name << ',' << t << ',' << tasks << ',' << qps << ',' << speedup << ','
              << speedup / static_cast<double>(t) << ',' << digest << '\n';
  }
}
}  // namespace

int main(int argc, char** argv) {
  const std::size_t vertices = argc > 1 ? std::stoull(argv[1]) : 50000;
  const std::size_t degree = argc > 2 ? std::stoull(argv[2]) : 8;
  const std::size_t tasks = argc > 3 ? std::stoull(argv[3]) : 32;
  const auto threads = parse_threads(argc > 4 ? argv[4] : "1,2,4");
  CsrGraph graph(make_edges(vertices, degree), false);

  std::cout << "algorithm,threads,tasks,throughput_per_s,speedup,parallel_efficiency,digest\n";
  emit_algorithm("bfs", tasks, threads, [&](std::size_t i) {
    const auto src = static_cast<VertexId>((i * 2654435761ULL) % graph.vertex_count());
    return hash_u32(velographx::bfs_distances(graph, src));
  });
  emit_algorithm("connected_components", tasks, threads, [&](std::size_t) {
    return hash_vid(velographx::connected_components(graph));
  });
  emit_algorithm("triangle_count", tasks, threads, [&](std::size_t) {
    return velographx::triangle_count(graph);
  });
  return 0;
}
