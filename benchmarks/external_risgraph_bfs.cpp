#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Edge = std::pair<velographx::VertexId, velographx::VertexId>;

std::vector<Edge> read_edges(const std::string& path, std::size_t& vertices) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list: " + path);
  std::vector<Edge> edges;
  std::string line;
  std::uint64_t max_vertex = 0;
  bool saw = false;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::uint64_t u = 0, v = 0;
    if (!(row >> u >> v)) continue;
    if (u > std::numeric_limits<velographx::VertexId>::max() ||
        v > std::numeric_limits<velographx::VertexId>::max()) {
      throw std::runtime_error("vertex id exceeds VeloGraphX VertexId range");
    }
    edges.emplace_back(static_cast<velographx::VertexId>(u),
                       static_cast<velographx::VertexId>(v));
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw = true;
  }
  vertices = saw ? static_cast<std::size_t>(max_vertex + 1) : 0;
  return edges;
}

std::vector<std::uint32_t> full_bfs(const velographx::DynamicGraph& graph,
                                    velographx::VertexId source) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> dist(graph.vertex_count(), unreachable);
  if (source >= graph.vertex_count()) return dist;
  std::queue<velographx::VertexId> q;
  dist[source] = 0;
  q.push(source);
  while (!q.empty()) {
    const auto u = q.front();
    q.pop();
    for (const auto v : graph.neighbors(u)) {
      if (dist[v] == unreachable) {
        dist[v] = dist[u] + 1;
        q.push(v);
      }
    }
  }
  return dist;
}

std::vector<std::uint64_t> layer_counts(const std::vector<std::uint32_t>& dist) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t max_depth = 0;
  for (const auto d : dist) if (d != unreachable) max_depth = std::max(max_depth, d);
  std::vector<std::uint64_t> counts(static_cast<std::size_t>(max_depth) + 1, 0);
  for (const auto d : dist) if (d != unreachable) ++counts[d];
  return counts;
}

void print_u64_array(const std::vector<std::uint64_t>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: " << argv[0]
              << " edge_list root imported_rate batch_size\n";
    return 2;
  }

  const std::string path = argv[1];
  const auto root64 = std::stoull(argv[2]);
  const double imported_rate = std::stod(argv[3]);
  const std::size_t batch_size = std::stoull(argv[4]);
  if (!(imported_rate > 0.0 && imported_rate < 1.0) || batch_size == 0 ||
      root64 > std::numeric_limits<velographx::VertexId>::max()) {
    std::cerr << "invalid arguments\n";
    return 2;
  }

  std::size_t vertices = 0;
  const auto edges = read_edges(path, vertices);
  const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
  if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size()) {
    std::cerr << "edge list/imported rate leaves no streaming window\n";
    return 2;
  }

  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);
  velographx::IncrementalBFS bfs(graph, static_cast<velographx::VertexId>(root64));

  std::size_t batches = 0;
  std::size_t operations = 0;
  std::size_t deletion_candidates = 0;
  std::size_t affected_vertices = 0;
  std::size_t full_recompute_batches = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t local_begin = imported_edges; local_begin < edges.size();
       local_begin += batch_size) {
    const auto local_end = std::min(local_begin + batch_size, edges.size());
    velographx::UpdateBatch updates;
    updates.updates.reserve((local_end - local_begin) * 2);
    for (std::size_t i = local_begin; i < local_end; ++i) {
      updates.add(edges[i].first, edges[i].second);
    }
    for (std::size_t i = local_begin; i < local_end; ++i) {
      const auto remove_index = i - imported_edges;
      updates.remove(edges[remove_index].first, edges[remove_index].second);
    }
    operations += updates.updates.size();
    bfs.apply(updates);
    deletion_candidates += bfs.last_deletion_candidates();
    affected_vertices += bfs.last_affected_vertices();
    full_recompute_batches += bfs.last_used_full_recompute() ? 1 : 0;
    ++batches;
  }
  const auto end = std::chrono::steady_clock::now();
  const double wall_us =
      std::chrono::duration<double, std::micro>(end - begin).count();

  const auto reference = full_bfs(graph, static_cast<velographx::VertexId>(root64));
  const bool correct = reference == bfs.distances();
  const auto layers = layer_counts(bfs.distances());
  std::size_t visited = 0;
  for (const auto count : layers) visited += count;

  std::cout << "{\"schema_version\":1,"
            << "\"artifact_type\":\"velographx-risgraph-bfs-comparison\","
            << "\"correct\":" << (correct ? "true" : "false") << ','
            << "\"directed\":true,"
            << "\"vertices\":" << vertices << ','
            << "\"source_edges\":" << edges.size() << ','
            << "\"initial_edges\":" << imported_edges << ','
            << "\"final_edges\":" << graph.edge_count_directed() << ','
            << "\"root\":" << root64 << ','
            << "\"imported_rate\":" << imported_rate << ','
            << "\"batch_size\":" << batch_size << ','
            << "\"batches\":" << batches << ','
            << "\"update_operations\":" << operations << ','
            << "\"deletion_candidates\":" << deletion_candidates << ','
            << "\"affected_vertices\":" << affected_vertices << ','
            << "\"full_recompute_batches\":" << full_recompute_batches << ','
            << "\"wall_us\":" << wall_us << ','
            << "\"wall_mean_us\":" << (batches ? wall_us / batches : 0.0) << ','
            << "\"visited_vertices\":" << visited << ','
            << "\"layer_counts\":";
  print_u64_array(layers);
  std::cout << "}\n";
  return correct ? 0 : 1;
}
