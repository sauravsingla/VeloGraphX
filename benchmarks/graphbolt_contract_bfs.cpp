#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Edge = std::pair<velographx::VertexId, velographx::VertexId>;
using Clock = std::chrono::steady_clock;

std::vector<Edge> read_edges(const std::string& path, std::uint64_t& max_vertex) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list: " + path);
  std::vector<Edge> edges;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '%') continue;
    std::istringstream row(line);
    std::uint64_t u = 0, v = 0;
    if (!(row >> u >> v)) continue;
    if (u > std::numeric_limits<velographx::VertexId>::max() ||
        v > std::numeric_limits<velographx::VertexId>::max()) {
      throw std::runtime_error("vertex id exceeds VertexId range");
    }
    max_vertex = std::max(max_vertex, std::max(u, v));
    edges.emplace_back(static_cast<velographx::VertexId>(u),
                       static_cast<velographx::VertexId>(v));
  }
  return edges;
}

velographx::UpdateBatch read_stream(const std::string& path,
                                    std::uint64_t& max_vertex,
                                    std::size_t& additions,
                                    std::size_t& deletions) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open update stream: " + path);
  velographx::UpdateBatch batch;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '%') continue;
    std::istringstream row(line);
    char op = 0;
    std::uint64_t u = 0, v = 0;
    if (!(row >> op >> u >> v)) throw std::runtime_error("malformed update row");
    if (u > std::numeric_limits<velographx::VertexId>::max() ||
        v > std::numeric_limits<velographx::VertexId>::max()) {
      throw std::runtime_error("vertex id exceeds VertexId range");
    }
    max_vertex = std::max(max_vertex, std::max(u, v));
    if (op == 'a') {
      batch.add(static_cast<velographx::VertexId>(u), static_cast<velographx::VertexId>(v));
      ++additions;
    } else if (op == 'd') {
      batch.remove(static_cast<velographx::VertexId>(u), static_cast<velographx::VertexId>(v));
      ++deletions;
    } else {
      throw std::runtime_error("unsupported update opcode");
    }
  }
  return batch;
}

std::vector<std::uint32_t> fresh_bfs(const velographx::DynamicGraph& graph,
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
    graph.for_each_neighbor(u, [&](velographx::VertexId v) {
      if (dist[v] == unreachable) {
        dist[v] = dist[u] + 1;
        q.push(v);
      }
    });
  }
  return dist;
}

std::size_t reachable(const std::vector<std::uint32_t>& dist) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::size_t count = 0;
  for (const auto d : dist) if (d != unreachable) ++count;
  return count;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: " << argv[0] << " INITIAL_EDGES UPDATE_STREAM ROOT\n";
    return 2;
  }
  const auto root64 = std::stoull(argv[3]);
  if (root64 > std::numeric_limits<velographx::VertexId>::max()) return 2;
  const auto root = static_cast<velographx::VertexId>(root64);

  try {
    std::uint64_t max_vertex = root64;
    auto initial = read_edges(argv[1], max_vertex);
    std::size_t additions = 0, deletions = 0;
    auto updates = read_stream(argv[2], max_vertex, additions, deletions);
    if (initial.empty() || updates.updates.empty()) return 2;

    velographx::DynamicGraph graph(static_cast<std::size_t>(max_vertex + 1), true);
    graph.bulk_load_edges(initial);
    velographx::IncrementalBFS bfs(graph, root, 2.0);

    const auto begin = Clock::now();
    bfs.apply(updates);
    const auto end = Clock::now();
    const double answer_ready_us =
        std::chrono::duration<double, std::micro>(end - begin).count();

    const auto reference = fresh_bfs(graph, root);
    const bool exact = reference == bfs.distances();
    std::cout << "{\"schema_version\":1"
              << ",\"system\":\"velographx_incremental_bfs\""
              << ",\"root\":" << root64
              << ",\"vertices\":" << graph.vertex_count()
              << ",\"initial_edges\":" << initial.size()
              << ",\"operations\":" << updates.updates.size()
              << ",\"additions\":" << additions
              << ",\"deletions\":" << deletions
              << ",\"answer_ready_us\":" << answer_ready_us
              << ",\"affected_vertices\":" << bfs.last_affected_vertices()
              << ",\"used_full_recompute\":" << (bfs.last_used_full_recompute() ? "true" : "false")
              << ",\"reachable_vertices\":" << reachable(bfs.distances())
              << ",\"exact\":" << (exact ? "true" : "false")
              << ",\"verification_excluded_from_timing\":true"
              << "}\n";
    return exact ? 0 : 1;
  } catch (const std::exception& exc) {
    std::cerr << "error: " << exc.what() << '\n';
    return 2;
  }
}
