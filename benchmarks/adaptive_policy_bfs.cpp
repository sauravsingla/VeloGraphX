#include <algorithm>
#include <chrono>
#include <cmath>
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

constexpr double kAffectedBudget = 0.05;
constexpr double kPreflightFullUpdate = 0.05;
constexpr double kVerySparseReach = 0.02;
constexpr double kSparseReach = 0.20;
constexpr double kSparseReachFullUpdate = 0.0025;
constexpr std::size_t kLargeGraphVertices = 100000;
constexpr double kLargeGraphUpdateGuard = 0.002;
constexpr std::uint32_t kShallowDeletionDepth = 3;
constexpr double kEmaAlpha = 0.25;
constexpr double kLearnedFullMargin = 1.25;
constexpr std::size_t kFreshAge = 4;

struct Trace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double shallow_parent_deletion_fraction{0.0};
  double previous_affected_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  std::size_t incremental_age{0};
  std::size_t full_age{0};
  bool chose_full{false};
  std::string reason;
};

struct PolicyResult {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<Trace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

std::vector<Edge> read_edges(const std::string& path, std::size_t& vertices) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list");
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
      throw std::runtime_error("vertex id exceeds VertexId range");
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

std::size_t reachable_vertices(const std::vector<std::uint32_t>& distances) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  return static_cast<std::size_t>(std::count_if(
      distances.begin(), distances.end(),
      [&](std::uint32_t x) { return x != unreachable; }));
}

velographx::UpdateBatch make_batch(const std::vector<Edge>& edges,
                                   std::size_t imported_edges,
                                   std::size_t begin,
                                   std::size_t end) {
  velographx::UpdateBatch updates;
  updates.updates.reserve((end - begin) * 2);
  for (std::size_t i = begin; i < end; ++i) {
    updates.add(edges[i].first, edges[i].second);
  }
  for (std::size_t i = begin; i < end; ++i) {
    const auto remove_index = i - imported_edges;
    updates.remove(edges[remove_index].first, edges[remove_index].second);
  }
  return updates;
}

double shallow_parent_deletion_fraction(const velographx::UpdateBatch& updates,
                                        const std::vector<std::uint32_t>& dist,
                                        bool directed) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::size_t shallow = 0;
  for (const auto& update : updates.updates) {
    if (update.add) continue;
    const auto u = update.src;
    const auto v = update.dst;
    if (u < dist.size() && v < dist.size() &&
        dist[u] != unreachable && dist[v] != unreachable &&
        dist[u] + 1 == dist[v] && dist[v] <= kShallowDeletionDepth) {
      ++shallow;
    }
    if (!directed && u < dist.size() && v < dist.size() &&
        dist[v] != unreachable && dist[u] != unreachable &&
        dist[v] + 1 == dist[u] && dist[u] <= kShallowDeletionDepth) {
      ++shallow;
    }
  }
  return static_cast<double>(shallow) /
      static_cast<double>(std::max<std::size_t>(1, updates.updates.size()));
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
  const auto initial_bfs_begin = Clock::now();
  const double repair_budget =
      policy == "always_incremental" ? 2.0 :
      ((policy == "adaptive" && vertices >= 200000) ? 2.0 : kAffectedBudget);
  velographx::IncrementalBFS bfs(graph, root, repair_budget);
  const auto initial_bfs_end = Clock::now();
  const double initial_bfs_us =
      std::chrono::duration<double, std::micro>(initial_bfs_end - initial_bfs_begin).count();

  PolicyResult result;
  result.name = policy;

  double initial_reachable_fraction = 0.0;
  if (policy == "adaptive") {
    const auto setup_begin = Clock::now();
    initial_reachable_fraction = static_cast<double>(reachable_vertices(bfs.distances())) /
        static_cast<double>(std::max<std::size_t>(1, vertices));
    const auto setup_end = Clock::now();
    result.selector_setup_us =
        std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();
  }

  double previous_affected_fraction = 0.0;
  double ema_incremental_us = 0.0;
  double ema_full_us = 0.0;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  bool have_incremental_error = false;
  bool have_full_error = false;
  const bool large_scale = vertices >= 200000;
  bool have_incremental = false;
  bool have_full = large_scale;
  if (large_scale) ema_full_us = initial_bfs_us;
  std::size_t incremental_age = kFreshAge + 1;
  std::size_t full_age = large_scale ? 0 : kFreshAge + 1;
  bool first_batch = true;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    bool choose_full = false;
    Trace trace;
    trace.reachable_fraction = initial_reachable_fraction;
    trace.previous_affected_fraction = previous_affected_fraction;
    trace.incremental_age = incremental_age;
    trace.full_age = full_age;

    const auto decision_begin = Clock::now();
    if (policy == "simple_threshold") {
      const double update_fraction = static_cast<double>(updates.updates.size()) /
          static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
      choose_full = update_fraction >= simple_update_fraction;
    } else if (policy == "adaptive") {
      const double update_fraction = static_cast<double>(updates.updates.size()) /
          static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
      const double shallow_fraction = first_batch
          ? shallow_parent_deletion_fraction(updates, bfs.distances(), graph.directed())
          : 0.0;
      trace.update_fraction = update_fraction;
      trace.shallow_parent_deletion_fraction = shallow_fraction;
      if (large_scale) {
        const double normalized_scale = have_incremental
            ? std::sqrt((update_fraction + 1e-12) /
                        (last_incremental_update_fraction + 1e-12))
            : 1.0;
        const double bounded_scale = std::clamp(normalized_scale, 0.50, 2.50);
        const double predicted_incremental = have_incremental
            ? ema_incremental_us * bounded_scale *
                  (1.0 + 3.0 * previous_affected_fraction)
            : 0.0;
        const double predicted_full = ema_full_us;
        const double inc_uncertainty = have_incremental_error
            ? std::clamp(ema_incremental_rel_error, 0.05, 0.35) : 0.35;
        const double full_uncertainty = have_full_error
            ? std::clamp(ema_full_rel_error, 0.05, 0.35) : 0.20;
        const double inc_lower = predicted_incremental * (1.0 - inc_uncertainty);
        const double full_upper = predicted_full * (1.0 + full_uncertainty);
        trace.predicted_incremental_us = predicted_incremental;
        trace.predicted_full_us = predicted_full;
        const bool shallow_cold_start = first_batch && shallow_fraction > 0.0;
        if (update_fraction >= kPreflightFullUpdate) {
          choose_full = true;
          trace.reason = "large_preflight_full";
        } else if (shallow_cold_start) {
          choose_full = true;
          trace.reason = "large_shallow_cold_start";
        } else if (!have_incremental) {
          choose_full = update_fraction >= simple_update_fraction;
          trace.reason = choose_full ? "large_warmup_full" : "large_warmup_incremental";
        } else if (inc_lower > full_upper) {
          choose_full = true;
          trace.reason = "large_uncertainty_confident_full";
        } else {
          choose_full = false;
          trace.reason = "large_uncertainty_overlap_incremental";
        }
      } else {
        const double predicted_incremental = have_incremental
            ? ema_incremental_us * (1.0 + 2.0 * previous_affected_fraction)
            : 0.0;
        const double predicted_full = have_full ? ema_full_us : 0.0;
        trace.predicted_incremental_us = predicted_incremental;
        trace.predicted_full_us = predicted_full;
        const bool fresh_model = have_incremental && have_full &&
            incremental_age <= kFreshAge && full_age <= kFreshAge;
        if (update_fraction >= kPreflightFullUpdate) {
          choose_full = true;
          trace.reason = "scale_preflight_full";
        } else if (initial_reachable_fraction <= kVerySparseReach) {
          choose_full = true;
          trace.reason = "scale_very_sparse_reach";
        } else if (initial_reachable_fraction < kSparseReach &&
                   update_fraction >= kSparseReachFullUpdate) {
          choose_full = true;
          trace.reason = "scale_sparse_reach_guard";
        } else if (fresh_model && predicted_incremental >
                                    predicted_full * kLearnedFullMargin) {
          choose_full = true;
          trace.reason = "scale_fresh_cost_model";
        } else {
          choose_full = false;
          trace.reason = fresh_model ? "scale_confidence_incremental" : "scale_insufficient_evidence_incremental";
        }
      }
      trace.chose_full = choose_full;
    }
    const auto decision_end = Clock::now();
    result.decision_us.push_back(
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count());

    const auto execution_begin = Clock::now();
    if (policy == "always_full" || choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++result.full_recompute_batches;
    } else {
      bfs.apply(updates);
      result.affected_vertices += bfs.last_affected_vertices();
      result.full_recompute_batches += bfs.last_used_full_recompute() ? 1 : 0;
    }
    const auto execution_end = Clock::now();
    const double execution_us =
        std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();
    double batch_us =
        std::chrono::duration<double, std::micro>(execution_end - total_begin).count();
    if (policy == "adaptive" && first_batch) batch_us += result.selector_setup_us;
    result.batch_us.push_back(batch_us);
    first_batch = false;

    if (policy == "adaptive") {
      ++incremental_age;
      ++full_age;
      const bool observed_full = choose_full || bfs.last_used_full_recompute();
      if (large_scale && trace.predicted_incremental_us > 0.0 &&
          trace.predicted_full_us > 0.0) {
        if (observed_full) {
          const double rel = std::abs(execution_us - trace.predicted_full_us) /
              std::max(1.0, trace.predicted_full_us);
          ema_full_rel_error = have_full_error
              ? (1.0 - kEmaAlpha) * ema_full_rel_error + kEmaAlpha * rel : rel;
          have_full_error = true;
        } else {
          const double rel = std::abs(execution_us - trace.predicted_incremental_us) /
              std::max(1.0, trace.predicted_incremental_us);
          ema_incremental_rel_error = have_incremental_error
              ? (1.0 - kEmaAlpha) * ema_incremental_rel_error + kEmaAlpha * rel : rel;
          have_incremental_error = true;
        }
      }
      if (observed_full) {
        ema_full_us = have_full
            ? (1.0 - kEmaAlpha) * ema_full_us + kEmaAlpha * execution_us
            : execution_us;
        have_full = true;
        full_age = 0;
        previous_affected_fraction = 0.0;
      } else {
        ema_incremental_us = have_incremental
            ? (1.0 - kEmaAlpha) * ema_incremental_us + kEmaAlpha * execution_us
            : execution_us;
        have_incremental = true;
        incremental_age = 0;
        last_incremental_update_fraction = trace.update_fraction;
        previous_affected_fraction = static_cast<double>(bfs.last_affected_vertices()) /
            static_cast<double>(std::max<std::size_t>(1, vertices));
      }
      result.traces.push_back(trace);
    }

    const auto reference = full_bfs(graph, root);
    if (reference != bfs.distances()) result.exact = false;
  }
  return result;
}

void print_array(const std::vector<double>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}

void print_trace_array(const std::vector<Trace>& traces) {
  std::cout << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& t = traces[i];
    std::cout << "{\"update_fraction\":" << t.update_fraction
              << ",\"reachable_fraction\":" << t.reachable_fraction
              << ",\"shallow_parent_deletion_fraction\":" << t.shallow_parent_deletion_fraction
              << ",\"previous_affected_fraction\":" << t.previous_affected_fraction
              << ",\"predicted_incremental_us\":" << t.predicted_incremental_us
              << ",\"predicted_full_us\":" << t.predicted_full_us
              << ",\"incremental_age\":" << t.incremental_age
              << ",\"full_age\":" << t.full_age
              << ",\"chose_full\":" << (t.chose_full ? "true" : "false")
              << ",\"reason\":\"" << t.reason << "\"}";
  }
  std::cout << ']';
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) return 2;
  const std::string path = argv[1];
  const auto root64 = std::stoull(argv[2]);
  const double imported_rate = std::stod(argv[3]);
  const auto batch_size = static_cast<std::size_t>(std::stoull(argv[4]));
  const double simple_update_fraction = std::stod(argv[5]);
  if (root64 > std::numeric_limits<velographx::VertexId>::max()) return 2;
  const auto root = static_cast<velographx::VertexId>(root64);

  std::size_t vertices = 0;
  const auto edges = read_edges(path, vertices);
  const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
  if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size() || batch_size == 0) return 2;

  const std::vector<std::string> names = {
      "always_incremental", "always_full", "simple_threshold", "adaptive"};
  std::vector<PolicyResult> results;
  for (const auto& name : names) {
    results.push_back(run_policy(name, edges, vertices, root, imported_edges,
                                 batch_size, simple_update_fraction));
  }

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, std::numeric_limits<double>::infinity());
  for (const auto& result : results) {
    for (std::size_t i = 0; i < batches; ++i) {
      oracle[i] = std::min(oracle[i], result.batch_us[i]);
    }
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":6,\"selector\":\"scale-conditioned-selector-owned-v3\""
            << ",\"root\":" << root64
            << ",\"vertices\":" << vertices
            << ",\"batch_size\":" << batch_size
            << ",\"batches\":" << batches
            << ",\"thresholds\":{\"affected_budget\":" << kAffectedBudget
            << ",\"preflight_full_update\":" << kPreflightFullUpdate
            << ",\"very_sparse_reach\":" << kVerySparseReach
            << ",\"sparse_reach\":" << kSparseReach
            << ",\"sparse_reach_full_update\":" << kSparseReachFullUpdate
            << ",\"large_graph_vertices\":" << kLargeGraphVertices
            << ",\"large_graph_update_guard\":" << kLargeGraphUpdateGuard
            << ",\"shallow_deletion_depth\":" << kShallowDeletionDepth
            << ",\"learned_full_margin\":" << kLearnedFullMargin
            << ",\"fresh_age\":" << kFreshAge << "}"
            << ",\"policies\":[";

  for (std::size_t p = 0; p < results.size(); ++p) {
    const auto& result = results[p];
    if (p) std::cout << ',';
    double total_us = 0.0;
    double decision_us = 0.0;
    for (double x : result.batch_us) total_us += x;
    for (double x : result.decision_us) decision_us += x;
    all_exact = all_exact && result.exact;
    std::cout << "{\"name\":\"" << result.name << "\""
              << ",\"exact\":" << (result.exact ? "true" : "false")
              << ",\"total_us\":" << total_us
              << ",\"mean_batch_us\":" << total_us / std::max<std::size_t>(1, batches)
              << ",\"selector_setup_us\":" << result.selector_setup_us
              << ",\"mean_decision_us\":" << decision_us / std::max<std::size_t>(1, batches)
              << ",\"full_recompute_batches\":" << result.full_recompute_batches
              << ",\"affected_vertices\":" << result.affected_vertices
              << ",\"batch_us\":";
    print_array(result.batch_us);
    if (result.name == "adaptive") {
      std::cout << ",\"trace\":";
      print_trace_array(result.traces);
    }
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}