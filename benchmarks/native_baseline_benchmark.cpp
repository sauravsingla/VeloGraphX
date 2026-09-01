#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "velographx/algorithms.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/io.hpp"
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

namespace {
using clock_type = std::chrono::steady_clock;
using velographx::EdgeWeight;
using velographx::VertexId;

std::uint64_t elapsed_us(clock_type::time_point a, clock_type::time_point b) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(b - a).count());
}

template <class T>
std::uint64_t digest(const std::vector<T>& values) {
  std::uint64_t h = 1469598103934665603ULL;
  for (const auto value : values) {
    std::uint64_t x = static_cast<std::uint64_t>(value);
    for (int i = 0; i < 8; ++i) {
      h ^= (x & 0xffU);
      h *= 1099511628211ULL;
      x >>= 8;
    }
  }
  return h;
}

struct WeightedInput {
  std::size_t vertices{};
  std::vector<std::tuple<VertexId, VertexId, EdgeWeight>> edges;
};

WeightedInput read_weighted_edges(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open weighted edge list: " + path);
  WeightedInput out;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::uint64_t u, v, w;
    if (!(row >> u >> v >> w)) continue;
    out.edges.emplace_back(static_cast<VertexId>(u), static_cast<VertexId>(v),
                           static_cast<EdgeWeight>(w));
    out.vertices = std::max<std::size_t>(out.vertices, std::max(u, v) + 1);
  }
  return out;
}

velographx::WeightedDynamicGraph build_weighted(const WeightedInput& input, bool directed = false) {
  velographx::WeightedDynamicGraph graph(input.vertices, directed);
  velographx::WeightedUpdateBatch batch;
  batch.updates.reserve(input.edges.size());
  for (const auto& [u, v, w] : input.edges) batch.add(u, v, w);
  graph.apply(batch);
  return graph;
}

std::vector<std::uint64_t> serial_dijkstra(const velographx::WeightedDynamicGraph& graph,
                                           VertexId source) {
  constexpr auto inf = velographx::IncrementalWeightedSSSP::kInf;
  std::vector<std::uint64_t> dist(graph.vertex_count(), inf);
  using Item = std::pair<std::uint64_t, VertexId>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
  if (source < dist.size()) {
    dist[source] = 0;
    q.push({0, source});
  }
  while (!q.empty()) {
    const auto [du, u] = q.top();
    q.pop();
    if (du != dist[u]) continue;
    graph.for_each_neighbor(u, [&](VertexId v, EdgeWeight w) {
      if (du <= inf - w && du + w < dist[v]) {
        dist[v] = du + w;
        q.push({dist[v], v});
      }
    });
  }
  return dist;
}

velographx::DynamicGraph read_dynamic_unweighted(const std::string& path, bool directed = false) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list: " + path);
  std::vector<std::pair<VertexId, VertexId>> edges;
  std::size_t vertices = 0;
  std::uint64_t u, v;
  while (in >> u >> v) {
    edges.emplace_back(static_cast<VertexId>(u), static_cast<VertexId>(v));
    vertices = std::max<std::size_t>(vertices, std::max(u, v) + 1);
  }
  velographx::DynamicGraph graph(vertices, directed);
  graph.bulk_load_edges(edges);
  return graph;
}

velographx::UpdateBatch read_unweighted_updates(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open update list: " + path);
  velographx::UpdateBatch batch;
  std::uint64_t u, v;
  while (in >> u >> v) batch.add(static_cast<VertexId>(u), static_cast<VertexId>(v));
  return batch;
}

velographx::WeightedUpdateBatch read_weighted_updates(const std::string& path) {
  const auto input = read_weighted_edges(path);
  velographx::WeightedUpdateBatch batch;
  batch.updates.reserve(input.edges.size());
  for (const auto& [u, v, w] : input.edges) batch.add(u, v, w);
  return batch;
}

void print_json(const std::string& algorithm, VertexId source, std::uint64_t kernel_us,
                std::uint64_t result_digest, bool exact, std::size_t vertices,
                const std::string& extra = "") {
  std::cout << "{\"algorithm\":\"" << algorithm << "\",\"source\":" << source
            << ",\"kernel_us\":" << kernel_us << ",\"digest\":" << result_digest
            << ",\"exact\":" << (exact ? "true" : "false")
            << ",\"vertices\":" << vertices;
  if (!extra.empty()) std::cout << ',' << extra;
  std::cout << "}\n";
}

int static_bfs(const std::string& path, VertexId source, bool directed = false) {
  const auto graph = velographx::load_edge_list(path, directed);
  const auto begin = clock_type::now();
  const auto dist = velographx::bfs_distances(graph, source);
  const auto end = clock_type::now();
  print_json(directed ? "bfs_directed" : "bfs", source, elapsed_us(begin, end), digest(dist), true,
             graph.vertex_count());
  return 0;
}

int static_sssp(const std::string& path, VertexId source, bool directed = false) {
  const auto input = read_weighted_edges(path);
  auto graph = build_weighted(input, directed);
  const auto begin = clock_type::now();
  velographx::IncrementalWeightedSSSP sssp(graph, source);
  const auto end = clock_type::now();
  const auto oracle = serial_dijkstra(graph, source);
  const bool exact = sssp.distances() == oracle;
  print_json(directed ? "sssp_directed" : "sssp", source, elapsed_us(begin, end),
             digest(sssp.distances()), exact, graph.vertex_count());
  return exact ? 0 : 3;
}

int dynamic_bfs(const std::string& base, const std::string& updates, VertexId source) {
  auto graph = read_dynamic_unweighted(base);
  velographx::IncrementalBFS bfs(graph, source);
  const auto batch = read_unweighted_updates(updates);
  const auto begin = clock_type::now();
  bfs.apply(batch);
  const auto end = clock_type::now();
  const auto full_begin = clock_type::now();
  velographx::IncrementalBFS full(graph, source);
  const auto full_end = clock_type::now();
  const bool exact = bfs.distances() == full.distances();
  print_json("dynamic_bfs", source, elapsed_us(begin, end), digest(bfs.distances()), exact,
             graph.vertex_count(), "\"full_recompute_us\":" +
                 std::to_string(elapsed_us(full_begin, full_end)) +
                 ",\"update_count\":" + std::to_string(batch.updates.size()));
  return exact ? 0 : 3;
}

int dynamic_sssp(const std::string& base, const std::string& updates, VertexId source) {
  auto graph = build_weighted(read_weighted_edges(base));
  velographx::IncrementalWeightedSSSP sssp(graph, source);
  const auto batch = read_weighted_updates(updates);
  const auto begin = clock_type::now();
  sssp.apply(batch);
  const auto end = clock_type::now();
  const auto full_begin = clock_type::now();
  velographx::IncrementalWeightedSSSP full(graph, source);
  const auto full_end = clock_type::now();
  const bool exact = sssp.distances() == full.distances() &&
                     sssp.distances() == serial_dijkstra(graph, source);
  print_json("dynamic_sssp", source, elapsed_us(begin, end), digest(sssp.distances()), exact,
             graph.vertex_count(), "\"full_recompute_us\":" +
                 std::to_string(elapsed_us(full_begin, full_end)) +
                 ",\"update_count\":" + std::to_string(batch.updates.size()));
  return exact ? 0 : 3;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 4) {
      std::cerr << "usage: velographx_native_baseline_benchmark "
                   "<bfs|bfs-directed|sssp|sssp-directed|dynamic-bfs|dynamic-sssp> "
                   "<input> [updates] <source>\n";
      return 2;
    }
    const std::string mode = argv[1];
    if (mode == "bfs" || mode == "bfs-directed" || mode == "sssp" || mode == "sssp-directed") {
      const auto source = static_cast<VertexId>(std::stoul(argv[3]));
      if (mode == "bfs") return static_bfs(argv[2], source, false);
      if (mode == "bfs-directed") return static_bfs(argv[2], source, true);
      if (mode == "sssp") return static_sssp(argv[2], source, false);
      return static_sssp(argv[2], source, true);
    }
    if (argc != 5) return 2;
    const auto source = static_cast<VertexId>(std::stoul(argv[4]));
    if (mode == "dynamic-bfs") return dynamic_bfs(argv[2], argv[3], source);
    if (mode == "dynamic-sssp") return dynamic_sssp(argv[2], argv[3], source);
    return 2;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 4;
  }
}
