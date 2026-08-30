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
using Clock = std::chrono::steady_clock;

struct PolicyResult {
  std::string name;
  std::vector<double> batch_us;
  std::size_t full_recompute_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

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
    graph.for_each_neighbor(u, [&](velographx::VertexId v) {
      if (dist[v] == unreachable) {
        dist[v] = dist[u] + 1;
        q.push(v);
      }
    });
  }
  return dist;
}

velographx::UpdateBatch make_batch(const std::vector<Edge>& edges,
                                   std::size_t imported_edges,
                                   std::size_t local_begin,
                                   std::size_t local_end) {
  velographx::UpdateBatch updates;
  updates.updates.reserve((local_end - local_begin) * 2);
  for (std::size_t i = local_begin; i < local_end; ++i) updates.add(edges[i].first, edges[i].second);
  for (std::size_t i = local_begin; i < local_end; ++i) {
    const auto remove_index = i - imported_edges;
    updates.remove(edges[remove_index].first, edges[remove_index].second);
  }
  return updates;
}

PolicyResult run_policy(const std::string& policy,
                        const std::vector<Edge>& edges,
                        std::size_t vertices,
                        velographx::VertexId root,
                        std::size_t imported_edges,
                        std::size_t batch_size,
                        double simple_update_fraction) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);

  const double fallback_fraction = policy == "always_incremental" ? 2.0 : 0.35;
  velographx::IncrementalBFS bfs(graph, root, fallback_fraction);
  PolicyResult result;
  result.name = policy;

  for (std::size_t local_begin = imported_edges; local_begin < edges.size(); local_begin += batch_size) {
    const auto local_end = std::min(local_begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, local_begin, local_end);

    const auto begin = Clock::now();
    if (policy == "always_full") {
      graph.apply(updates);
      bfs.recompute();
      ++result.full_recompute_batches;
    } else if (policy == "simple_threshold") {
      const auto denominator = std::max<std::size_t>(1, graph.edge_count_directed());
      const double update_fraction = static_cast<double>(updates.updates.size()) /
                                     static_cast<double>(denominator);
      if (update_fraction >= simple_update_fraction) {
        graph.apply(updates);
        bfs.recompute();
        ++result.full_recompute_batches;
      } else {
        bfs.apply(updates);
        result.affected_vertices += bfs.last_affected_vertices();
      }
    } else {
      bfs.apply(updates);
      result.affected_vertices += bfs.last_affected_vertices();
      result.full_recompute_batches += bfs.last_used_full_recompute() ? 1 : 0;
    }
    const auto end = Clock::now();
    result.batch_us.push_back(std::chrono::duration<double, std::micro>(end - begin).count());

    const auto reference = full_bfs(graph, root);
    if (reference != bfs.distances()) result.exact = false;
  }
  return result;
}

void print_double_array(const std::vector<double>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    std::cerr << "usage: " << argv[0]
              << " edge_list root imported_rate batch_size simple_update_fraction\n";
    return 2;
  }

  const std::string path = argv[1];
  const auto root64 = std::stoull(argv[2]);
  const double imported_rate = std::stod(argv[3]);
  const std::size_t batch_size = std::stoull(argv[4]);
  const double simple_update_fraction = std::stod(argv[5]);
  if (!(imported_rate > 0.0 && imported_rate < 1.0) || batch_size == 0 ||
      !(simple_update_fraction > 0.0 && simple_update_fraction < 1.0) ||
      root64 > std::numeric_limits<velographx::VertexId>::max()) {
    std::cerr << "invalid arguments\n";
    return 2;
  }
  const auto root = static_cast<velographx::VertexId>(root64);

  std::size_t vertices = 0;
  const auto edges = read_edges(path, vertices);
  const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
  if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size()) {
    std::cerr << "edge list/imported rate leaves no streaming window\n";
    return 2;
  }

  const std::vector<std::string> policies = {
      "always_incremental", "always_full", "simple_threshold", "adaptive"};
  std::vector<PolicyResult> results;
  results.reserve(policies.size());
  for (const auto& policy : policies) {
    results.push_back(run_policy(policy, edges, vertices, root, imported_edges,
                                 batch_size, simple_update_fraction));
  }

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, std::numeric_limits<double>::infinity());
  for (const auto& r : results) {
    if (r.batch_us.size() != batches) return 1;
    for (std::size_t i = 0; i < batches; ++i) oracle[i] = std::min(oracle[i], r.batch_us[i]);
  }
  double oracle_total = 0.0;
  for (const auto value : oracle) oracle_total += value;

  bool all_exact = true;
  std::cout << "{\"schema_version\":1,\"artifact_type\":\"velographx-adaptive-policy-ablation\","
            << "\"root\":" << root64 << ",\"vertices\":" << vertices
            << ",\"source_edges\":" << edges.size() << ",\"initial_edges\":" << imported_edges
            << ",\"imported_rate\":" << imported_rate << ",\"batch_size\":" << batch_size
            << ",\"simple_update_fraction\":" << simple_update_fraction
            << ",\"adaptive_affected_fraction\":0.35,\"batches\":" << batches
            << ",\"verification_excluded_from_timing\":true,\"policies\":[";

  for (std::size_t p = 0; p < results.size(); ++p) {
    const auto& r = results[p];
    if (p) std::cout << ',';
    double total = 0.0;
    for (const auto value : r.batch_us) total += value;
    const double regret = oracle_total > 0.0 ? (total - oracle_total) / oracle_total : 0.0;
    all_exact = all_exact && r.exact;
    std::cout << "{\"name\":\"" << r.name << "\",\"exact\":" << (r.exact ? "true" : "false")
              << ",\"total_us\":" << total << ",\"mean_batch_us\":" << (batches ? total / batches : 0.0)
              << ",\"regret_vs_batch_oracle\":" << regret
              << ",\"full_recompute_batches\":" << r.full_recompute_batches
              << ",\"affected_vertices\":" << r.affected_vertices << ",\"batch_us\":";
    print_double_array(r.batch_us);
    std::cout << '}';
  }

  std::cout << "],\"oracle_total_us\":" << oracle_total << ",\"oracle_batch_us\":";
  print_double_array(oracle);
  std::cout << ",\"all_policies_exact\":" << (all_exact ? "true" : "false") << "}\n";
  return all_exact ? 0 : 1;
}
