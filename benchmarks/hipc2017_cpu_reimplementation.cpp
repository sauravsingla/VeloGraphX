#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Edge = std::pair<velographx::VertexId, velographx::VertexId>;
using Adj = std::vector<std::vector<velographx::VertexId>>;
using Clock = std::chrono::steady_clock;

std::vector<Edge> load_edges(const std::string& path, std::size_t& vertex_count) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("failed to open edge list: " + path);
  std::vector<Edge> edges;
  velographx::VertexId max_vertex = 0;
  bool saw_edge = false;
  std::string line;
  while (std::getline(in, line)) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') continue;
    std::istringstream row(line);
    velographx::VertexId u = 0, v = 0;
    if (!(row >> u >> v)) throw std::runtime_error("malformed edge-list row");
    if (u == v) continue;
    if (u > v) std::swap(u, v);
    edges.emplace_back(u, v);
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw_edge = true;
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  vertex_count = saw_edge ? static_cast<std::size_t>(max_vertex) + 1 : 0;
  return edges;
}

std::uint64_t edge_key(velographx::VertexId u, velographx::VertexId v) {
  if (u > v) std::swap(u, v);
  return (static_cast<std::uint64_t>(u) << 32U) | static_cast<std::uint64_t>(v);
}

velographx::DynamicGraph make_vx_graph(std::size_t n, const std::vector<Edge>& edges) {
  velographx::DynamicGraph graph(n, false);
  velographx::UpdateBatch seed;
  seed.updates.reserve(edges.size());
  for (const auto& [u, v] : edges) seed.add(u, v);
  graph.apply(seed);
  graph.compact();
  return graph;
}

velographx::UpdateBatch make_updates(const velographx::DynamicGraph& graph,
                                     std::size_t requested,
                                     std::uint64_t seed) {
  velographx::UpdateBatch batch;
  batch.updates.reserve(requested);
  std::unordered_set<std::uint64_t> selected;
  selected.reserve(requested * 2 + 1);
  const auto n = static_cast<std::uint64_t>(graph.vertex_count());
  std::uint64_t state = seed | 1ULL;
  const std::size_t max_attempts = std::max<std::size_t>(requested * 200, 10000);
  std::size_t attempts = 0;
  while (batch.updates.size() < requested && attempts++ < max_attempts) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    const auto u = static_cast<velographx::VertexId>((state >> 17) % n);
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    const auto v = static_cast<velographx::VertexId>((state >> 17) % n);
    if (u == v || graph.has_edge(u, v)) continue;
    const auto key = edge_key(u, v);
    if (selected.insert(key).second) batch.add(u, v);
  }
  if (batch.updates.size() != requested) {
    throw std::runtime_error("could not generate requested missing-edge batch");
  }
  return batch;
}

Adj build_adjacency(std::size_t n, const std::vector<Edge>& edges) {
  Adj adj(n);
  for (const auto& [u, v] : edges) {
    adj[u].push_back(v);
    adj[v].push_back(u);
  }
  for (auto& list : adj) {
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
  }
  return adj;
}

Adj build_update_adjacency(std::size_t n, const velographx::UpdateBatch& batch) {
  Adj delta(n);
  for (const auto& op : batch.updates) {
    if (!op.add) throw std::runtime_error("HiPC insertion comparator accepts insertions only");
    delta[op.src].push_back(op.dst);
    delta[op.dst].push_back(op.src);
  }
  for (auto& list : delta) std::sort(list.begin(), list.end());
  return delta;
}

void insert_sorted(std::vector<velographx::VertexId>& list, velographx::VertexId value) {
  auto it = std::lower_bound(list.begin(), list.end(), value);
  if (it == list.end() || *it != value) list.insert(it, value);
}

void apply_batch_to_adjacency(Adj& post, const velographx::UpdateBatch& batch) {
  for (const auto& op : batch.updates) {
    insert_sorted(post[op.src], op.dst);
    insert_sorted(post[op.dst], op.src);
  }
}

std::uint64_t intersection_count(const std::vector<velographx::VertexId>& a,
                                 const std::vector<velographx::VertexId>& b) {
  std::size_t i = 0, j = 0;
  std::uint64_t count = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) {
      ++count; ++i; ++j;
    } else if (a[i] < b[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  return count;
}

// CPU reimplementation of the HiPC 2017 insertion inclusion-exclusion formulation.
// The paper's Algorithm 2 computes three phases over the update edges:
//   S1: post/post intersections
//   S2: post/update intersections
//   S3: update/update intersections
// and returns 1/2 * (S1 - S2 + S3/3) in its directed-edge accounting.
// Here each undirected update edge is stored once, so S1 is half the paper's
// directed representation while S2/S3 explicitly sum both orientations. The
// equivalent undirected formula is: delta = S1 - S2/2 + S3/6.
std::uint64_t hipc2017_insert_delta(const Adj& post,
                                    const Adj& delta,
                                    const velographx::UpdateBatch& batch,
                                    std::uint64_t& s1,
                                    std::uint64_t& s2,
                                    std::uint64_t& s3) {
  s1 = s2 = s3 = 0;
  for (const auto& op : batch.updates) {
    const auto u = op.src;
    const auto v = op.dst;
    s1 += intersection_count(post[u], post[v]);
    s2 += intersection_count(post[u], delta[v]);
    s2 += intersection_count(post[v], delta[u]);
    s3 += intersection_count(delta[u], delta[v]);
    s3 += intersection_count(delta[v], delta[u]);
  }
  if ((s2 % 2U) != 0U || (s3 % 6U) != 0U) {
    throw std::runtime_error("HiPC inclusion-exclusion accounting invariant failed");
  }
  return s1 - s2 / 2U + s3 / 6U;
}

long long ns(Clock::time_point begin, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) {
    std::cerr << "usage: hipc2017_cpu_reimplementation <edge-list> [repeats] [fractions-csv]\n";
    return 2;
  }
  const std::string dataset = argv[1];
  const int repeats = argc >= 3 ? std::stoi(argv[2]) : 5;
  if (repeats <= 0) return 2;

  std::vector<double> fractions{0.01, 0.05, 0.10};
  if (argc == 4) {
    fractions.clear();
    std::istringstream input(argv[3]);
    std::string token;
    while (std::getline(input, token, ',')) {
      const double f = std::stod(token);
      if (!(f > 0.0) || !std::isfinite(f)) return 2;
      fractions.push_back(f);
    }
  }

  std::size_t vertex_count = 0;
  const auto edges = load_edges(dataset, vertex_count);
  if (edges.empty()) return 2;
  const Adj base_adj = build_adjacency(vertex_count, edges);

  std::cout << "dataset,vertices,base_edges,update_fraction,requested_edges,repeat,"
               "velographx_update_ns,velographx_full_recompute_ns,"
               "hipc_graph_update_ns,hipc_analytic_ns,hipc_answer_ready_ns,"
               "velographx_updates_per_s,hipc_answer_ready_updates_per_s,"
               "initial_triangles,velographx_triangles,hipc_triangles,recomputed_triangles,"
               "hipc_s1,hipc_s2,hipc_s3,correct\n";

  for (double fraction : fractions) {
    const std::size_t requested = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(static_cast<double>(edges.size()) * fraction)));

    for (int repeat = 0; repeat < repeats; ++repeat) {
      auto graph = make_vx_graph(vertex_count, edges);
      velographx::IncrementalTriangleCount triangles(graph);
      const auto initial_value = triangles.value();
      const auto update = make_updates(
          graph, requested,
          0x9e3779b97f4a7c15ULL ^
              (static_cast<std::uint64_t>(repeat + 1) * 0xbf58476d1ce4e5b9ULL) ^
              static_cast<std::uint64_t>(requested));

      const auto vx_begin = Clock::now();
      triangles.apply(update);
      const auto vx_end = Clock::now();
      const auto vx_value = triangles.value();

      const auto full_begin = Clock::now();
      triangles.recompute();
      const auto full_end = Clock::now();
      const auto full_value = triangles.value();

      Adj post = base_adj;
      const Adj delta = build_update_adjacency(vertex_count, update);
      const auto hipc_update_begin = Clock::now();
      apply_batch_to_adjacency(post, update);
      const auto hipc_update_end = Clock::now();

      std::uint64_t s1 = 0, s2 = 0, s3 = 0;
      const auto hipc_analytic_begin = Clock::now();
      const auto added = hipc2017_insert_delta(post, delta, update, s1, s2, s3);
      const auto hipc_analytic_end = Clock::now();
      const auto hipc_value = initial_value + added;

      const auto vx_ns = ns(vx_begin, vx_end);
      const auto full_ns = ns(full_begin, full_end);
      const auto hipc_update_ns = ns(hipc_update_begin, hipc_update_end);
      const auto hipc_analytic_ns = ns(hipc_analytic_begin, hipc_analytic_end);
      const auto hipc_ready_ns = hipc_update_ns + hipc_analytic_ns;
      const bool correct = vx_value == full_value && hipc_value == full_value;
      if (!correct) {
        std::cerr << "exact-count disagreement: VeloGraphX=" << vx_value
                  << " HiPC-derived=" << hipc_value << " full=" << full_value << '\n';
        return 1;
      }

      const double vx_rate = vx_ns > 0 ? static_cast<double>(requested) * 1e9 / vx_ns : 0.0;
      const double hipc_rate = hipc_ready_ns > 0
          ? static_cast<double>(requested) * 1e9 / hipc_ready_ns : 0.0;

      std::cout << dataset << ',' << vertex_count << ',' << edges.size() << ',' << fraction << ','
                << requested << ',' << repeat << ',' << vx_ns << ',' << full_ns << ','
                << hipc_update_ns << ',' << hipc_analytic_ns << ',' << hipc_ready_ns << ','
                << vx_rate << ',' << hipc_rate << ',' << initial_value << ',' << vx_value << ','
                << hipc_value << ',' << full_value << ',' << s1 << ',' << s2 << ',' << s3 << ','
                << (correct ? 1 : 0) << '\n';
    }
  }
  return 0;
}
