// Publication BFS selector v3 (bounded-probe revision).
//
// Historical v1/v2 and the first v3 development harness remain untouched.
// This revision replaces broad depth-tail forcing with a bounded pre-repair
// propagation probe. The probe explores only enough shortest-path invalidation
// and insertion-relaxation work to establish a lower-bound work signal, then
// stops at a scale-aware budget. Its full cost is charged to selector latency.
// Hosted timings remain engineering evidence only (research_claim=false).
#define main velographx_adaptive_policy_helpers_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace {

struct ProbeResult {
  std::size_t deletion_changes{0};
  std::size_t insertion_changes{0};
  std::size_t budget{0};
  bool saturated{false};
};

struct ProbeTrace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double previous_affected_fraction{0.0};
  double probe_vertex_fraction{0.0};
  double probe_amplification{0.0};
  std::size_t deletion_probe_vertices{0};
  std::size_t insertion_probe_vertices{0};
  std::size_t probe_budget{0};
  bool probe_saturated{false};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  bool internal_full_fallback{false};
  std::string reason;
};

struct ProbePolicyResult {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<double> execution_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_full_fallback;
  std::vector<ProbeTrace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

std::uint64_t edge_key_probe(velographx::VertexId u, velographx::VertexId v) {
  return (static_cast<std::uint64_t>(u) << 32) | static_cast<std::uint64_t>(v);
}

std::uint32_t sparse_distance(
    velographx::VertexId v,
    const std::vector<std::uint32_t>& dist,
    const std::unordered_map<velographx::VertexId, std::uint32_t>& overrides) {
  if (const auto it = overrides.find(v); it != overrides.end()) return it->second;
  return v < dist.size() ? dist[v] : std::numeric_limits<std::uint32_t>::max();
}

ProbeResult bounded_preflight_probe(
    const velographx::DynamicGraph& graph,
    const velographx::UpdateBatch& updates,
    const std::vector<std::uint32_t>& dist) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  ProbeResult result;
  const auto vertices = std::max<std::size_t>(1, graph.vertex_count());
  const auto operations = std::max<std::size_t>(1, updates.updates.size());

  // Scale-aware hard bound: at most 8 probe vertices per update and never more
  // than 10% of the graph. The cap is a selector-work budget, not a claim that
  // 10% is a universal repair/recompute crossover.
  result.budget = std::max<std::size_t>(64, std::min<std::size_t>(
      std::max<std::size_t>(64, vertices / 10), operations * 8));

  std::vector<std::pair<velographx::VertexId, velographx::VertexId>> additions;
  std::vector<std::pair<velographx::VertexId, velographx::VertexId>> deletions;
  additions.reserve(updates.updates.size());
  deletions.reserve(updates.updates.size());
  std::unordered_set<std::uint64_t> deleted;
  deleted.reserve(updates.updates.size() * 2 + 1);
  std::unordered_map<velographx::VertexId, std::vector<velographx::VertexId>> added_from;

  for (const auto& update : updates.updates) {
    if (update.add) {
      additions.emplace_back(update.src, update.dst);
      added_from[update.src].push_back(update.dst);
    } else if (velographx::has_edge(graph, update.src, update.dst)) {
      deletions.emplace_back(update.src, update.dst);
      deleted.insert(edge_key_probe(update.src, update.dst));
    }
  }

  // Bounded deletion invalidation probe. This mirrors the exact BFS parent-loss
  // semantics but stops as soon as the selector work budget is reached.
  std::unordered_map<velographx::VertexId, std::size_t> lost;
  lost.reserve(deletions.size() * 2 + 8);
  std::unordered_map<velographx::VertexId, std::size_t> support_cache;
  support_cache.reserve(deletions.size() * 2 + 8);
  std::unordered_set<velographx::VertexId> affected;
  affected.reserve(std::min<std::size_t>(result.budget * 2 + 1, vertices));
  std::vector<velographx::VertexId> queue;
  queue.reserve(result.budget);

  auto shortest_support = [&](velographx::VertexId v) -> std::size_t {
    if (const auto it = support_cache.find(v); it != support_cache.end()) return it->second;
    if (v >= dist.size() || dist[v] == unreachable || dist[v] == 0) {
      support_cache.emplace(v, 0);
      return 0;
    }
    std::size_t count = 0;
    velographx::for_each_in_neighbor(graph, v, [&](velographx::VertexId p) {
      if (p < dist.size() && dist[p] != unreachable && dist[p] + 1 == dist[v]) ++count;
    });
    support_cache.emplace(v, count);
    return count;
  };

  auto record_loss = [&](velographx::VertexId v) {
    if (result.saturated || v >= dist.size() || dist[v] == unreachable || dist[v] == 0 ||
        affected.find(v) != affected.end()) return;
    const auto count = ++lost[v];
    const auto support = shortest_support(v);
    if (support != 0 && count >= support) {
      affected.insert(v);
      queue.push_back(v);
      ++result.deletion_changes;
      if (result.deletion_changes + result.insertion_changes >= result.budget) {
        result.saturated = true;
      }
    }
  };

  for (const auto& [u, v] : deletions) {
    if (u < dist.size() && v < dist.size() &&
        dist[u] != unreachable && dist[v] != unreachable && dist[u] + 1 == dist[v]) {
      record_loss(v);
      if (result.saturated) return result;
    }
  }

  for (std::size_t head = 0; head < queue.size() && !result.saturated; ++head) {
    const auto u = queue[head];
    if (u >= dist.size() || dist[u] == unreachable) continue;
    velographx::for_each_neighbor(graph, u, [&](velographx::VertexId v) {
      if (result.saturated || v >= dist.size() || dist[v] != dist[u] + 1) return;
      if (deleted.find(edge_key_probe(u, v)) != deleted.end()) return;
      record_loss(v);
    });
  }
  if (result.saturated) return result;

  // Bounded insertion-relaxation probe. Sparse distance overrides avoid an O(V)
  // distance copy. Existing edges scheduled for deletion are skipped; batch
  // additions are traversed as temporary outgoing edges.
  std::unordered_map<velographx::VertexId, std::uint32_t> tentative;
  tentative.reserve(std::min<std::size_t>(result.budget * 2 + 1, vertices));
  std::vector<velographx::VertexId> relax_queue;
  relax_queue.reserve(result.budget);

  auto relax = [&](velographx::VertexId u, velographx::VertexId v) {
    if (result.saturated) return;
    const auto du = sparse_distance(u, dist, tentative);
    if (du == unreachable) return;
    const auto old = sparse_distance(v, dist, tentative);
    if (du + 1 >= old) return;
    const bool first_change = tentative.find(v) == tentative.end();
    tentative[v] = du + 1;
    relax_queue.push_back(v);
    if (first_change) {
      ++result.insertion_changes;
      if (result.deletion_changes + result.insertion_changes >= result.budget) {
        result.saturated = true;
      }
    }
  };

  for (const auto& [u, v] : additions) {
    relax(u, v);
    if (result.saturated) return result;
  }

  for (std::size_t head = 0; head < relax_queue.size() && !result.saturated; ++head) {
    const auto u = relax_queue[head];
    velographx::for_each_neighbor(graph, u, [&](velographx::VertexId v) {
      if (result.saturated || deleted.find(edge_key_probe(u, v)) != deleted.end()) return;
      relax(u, v);
    });
    if (const auto it = added_from.find(u); it != added_from.end()) {
      for (auto v : it->second) {
        relax(u, v);
        if (result.saturated) break;
      }
    }
  }
  return result;
}

ProbePolicyResult run_baseline_probe(
    const std::string& policy,
    const std::vector<Edge>& edges,
    std::size_t vertices,
    velographx::VertexId root,
    std::size_t imported_edges,
    std::size_t batch_size,
    double simple_update_fraction) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);
  velographx::IncrementalBFS bfs(graph, root, 2.0);
  ProbePolicyResult r;
  r.name = policy;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();
    const double uf = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    const bool full = policy == "always_full" ||
        (policy == "simple_threshold" && uf >= simple_update_fraction);
    r.decision_us.push_back(0.0);
    const auto exec_begin = Clock::now();
    bool fallback = false;
    if (full) {
      graph.apply(updates);
      bfs.recompute();
      ++r.full_recompute_batches;
    } else {
      bfs.apply(updates);
      r.affected_vertices += bfs.last_affected_vertices();
      fallback = bfs.last_used_full_recompute();
      if (fallback) {
        ++r.full_recompute_batches;
        ++r.internal_fallback_batches;
      }
    }
    const auto exec_end = Clock::now();
    const double execution = std::chrono::duration<double, std::micro>(exec_end - exec_begin).count();
    r.execution_us.push_back(execution);
    r.batch_us.push_back(std::chrono::duration<double, std::micro>(exec_end - total_begin).count());
    r.explicit_full.push_back(full);
    r.internal_full_fallback.push_back(fallback);
    ProbeTrace t;
    t.update_fraction = uf;
    t.explicit_full = full;
    t.internal_full_fallback = fallback;
    t.reason = policy;
    r.traces.push_back(t);
    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
  }
  return r;
}

ProbePolicyResult run_adaptive_probe(
    const std::vector<Edge>& edges,
    std::size_t vertices,
    velographx::VertexId root,
    std::size_t imported_edges,
    std::size_t batch_size) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);
  const auto init_begin = Clock::now();
  velographx::IncrementalBFS bfs(graph, root, 2.0);
  const auto init_end = Clock::now();
  const double initial_full_us = std::chrono::duration<double, std::micro>(init_end - init_begin).count();

  ProbePolicyResult r;
  r.name = "adaptive";
  const auto setup_begin = Clock::now();
  const double reachable_fraction = static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  r.selector_setup_us = std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();

  double ema_inc = 0.0;
  double ema_full = initial_full_us;
  double ema_inc_error = 0.0;
  double ema_full_error = 0.0;
  double previous_affected_fraction = 0.0;
  double last_probe_work = 0.0;
  bool have_inc = false;
  bool have_inc_error = false;
  bool have_full_error = false;
  std::size_t inc_age = 0;
  std::size_t full_age = 0;
  bool first_batch = true;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    ProbeTrace t;
    t.reachable_fraction = reachable_fraction;
    t.previous_affected_fraction = previous_affected_fraction;
    t.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));

    const auto decision_begin = Clock::now();
    const auto probe = bounded_preflight_probe(graph, updates, bfs.distances());
    const double probe_work = static_cast<double>(probe.deletion_changes + probe.insertion_changes);
    const double operations = static_cast<double>(std::max<std::size_t>(1, updates.updates.size()));
    t.deletion_probe_vertices = probe.deletion_changes;
    t.insertion_probe_vertices = probe.insertion_changes;
    t.probe_budget = probe.budget;
    t.probe_saturated = probe.saturated;
    t.probe_vertex_fraction = probe_work / static_cast<double>(std::max<std::size_t>(1, vertices));
    t.probe_amplification = probe_work / operations;

    bool choose_full = false;
    if (!have_inc) {
      // Initial BFS already provides a measured full arm. Use one incremental
      // calibration unless the bounded probe itself saturates, which means the
      // batch has already exhausted the selector's preflight work budget.
      choose_full = probe.saturated;
      t.reason = choose_full ? "v3_probe_saturated_cold_full" : "v3_incremental_probe";
    } else {
      const double work_ratio = (probe_work + operations) /
          (last_probe_work + operations);
      const double work_scale = std::clamp(std::sqrt(work_ratio), 0.45, 3.00);
      t.predicted_incremental_us = ema_inc * work_scale *
          (1.0 + 2.0 * std::clamp(previous_affected_fraction, 0.0, 1.0));
      t.predicted_full_us = ema_full;

      const double inc_uncertainty = std::min(0.70,
          (have_inc_error ? std::clamp(ema_inc_error, 0.08, 0.50) : 0.40) +
          std::min(0.20, 0.02 * static_cast<double>(inc_age)));
      const double full_uncertainty = std::min(0.55,
          (have_full_error ? std::clamp(ema_full_error, 0.08, 0.40) : 0.25) +
          std::min(0.20, 0.015 * static_cast<double>(full_age)));
      const double inc_lower = t.predicted_incremental_us * (1.0 - inc_uncertainty);
      const double full_upper = t.predicted_full_us * (1.0 + full_uncertainty);
      const bool cost_separated_full = inc_lower > full_upper;
      choose_full = probe.saturated || cost_separated_full;
      if (probe.saturated) t.reason = "v3_probe_saturated_full";
      else if (cost_separated_full) t.reason = "v3_probe_cost_separated_full";
      else t.reason = "v3_probe_incremental";
    }
    t.explicit_full = choose_full;
    const auto decision_end = Clock::now();
    r.decision_us.push_back(std::chrono::duration<double, std::micro>(decision_end - decision_begin).count());

    const auto exec_begin = Clock::now();
    bool fallback = false;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++r.full_recompute_batches;
    } else {
      bfs.apply(updates);
      r.affected_vertices += bfs.last_affected_vertices();
      fallback = bfs.last_used_full_recompute();
      if (fallback) {
        ++r.full_recompute_batches;
        ++r.internal_fallback_batches;
      }
    }
    const auto exec_end = Clock::now();
    const double execution_us = std::chrono::duration<double, std::micro>(exec_end - exec_begin).count();
    double batch_us = std::chrono::duration<double, std::micro>(exec_end - total_begin).count();
    if (first_batch) batch_us += r.selector_setup_us;
    t.internal_full_fallback = fallback;
    r.execution_us.push_back(execution_us);
    r.batch_us.push_back(batch_us);
    r.explicit_full.push_back(choose_full);
    r.internal_full_fallback.push_back(fallback);
    r.traces.push_back(t);

    ++inc_age;
    ++full_age;
    const bool observed_full = choose_full || fallback;
    if (t.predicted_incremental_us > 0.0 && t.predicted_full_us > 0.0) {
      if (observed_full) {
        const double rel = std::abs(execution_us - t.predicted_full_us) /
            std::max(1.0, t.predicted_full_us);
        ema_full_error = have_full_error ? (1.0 - kEmaAlpha) * ema_full_error + kEmaAlpha * rel : rel;
        have_full_error = true;
      } else {
        const double rel = std::abs(execution_us - t.predicted_incremental_us) /
            std::max(1.0, t.predicted_incremental_us);
        ema_inc_error = have_inc_error ? (1.0 - kEmaAlpha) * ema_inc_error + kEmaAlpha * rel : rel;
        have_inc_error = true;
      }
    }
    if (observed_full) {
      ema_full = (1.0 - kEmaAlpha) * ema_full + kEmaAlpha * execution_us;
      full_age = 0;
      previous_affected_fraction = 0.0;
    } else {
      ema_inc = have_inc ? (1.0 - kEmaAlpha) * ema_inc + kEmaAlpha * execution_us : execution_us;
      have_inc = true;
      inc_age = 0;
      last_probe_work = probe_work;
      previous_affected_fraction = static_cast<double>(bfs.last_affected_vertices()) /
          static_cast<double>(std::max<std::size_t>(1, vertices));
    }

    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
    first_batch = false;
  }
  return r;
}

void print_probe_bool_array(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_probe_traces(const std::vector<ProbeTrace>& traces) {
  std::cout << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& t = traces[i];
    std::cout << "{\"update_fraction\":" << t.update_fraction
              << ",\"reachable_fraction\":" << t.reachable_fraction
              << ",\"previous_affected_fraction\":" << t.previous_affected_fraction
              << ",\"probe_vertex_fraction\":" << t.probe_vertex_fraction
              << ",\"probe_amplification\":" << t.probe_amplification
              << ",\"deletion_probe_vertices\":" << t.deletion_probe_vertices
              << ",\"insertion_probe_vertices\":" << t.insertion_probe_vertices
              << ",\"probe_budget\":" << t.probe_budget
              << ",\"probe_saturated\":" << (t.probe_saturated ? "true" : "false")
              << ",\"predicted_incremental_us\":" << t.predicted_incremental_us
              << ",\"predicted_full_us\":" << t.predicted_full_us
              << ",\"explicit_full\":" << (t.explicit_full ? "true" : "false")
              << ",\"internal_full_fallback\":" << (t.internal_full_fallback ? "true" : "false")
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

  std::vector<ProbePolicyResult> results;
  results.push_back(run_baseline_probe("always_incremental", edges, vertices, root, imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_baseline_probe("always_full", edges, vertices, root, imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_baseline_probe("simple_threshold", edges, vertices, root, imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_adaptive_probe(edges, vertices, root, imported_edges, batch_size));

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":5"
            << ",\"artifact_type\":\"velographx-publication-repair-recompute-policy\""
            << ",\"selector\":\"publication-preflight-bfs-v3-probe\""
            << ",\"parent_selector\":\"publication-preflight-v2\""
            << ",\"selector_change\":\"bounded pre-repair shortest-path invalidation and insertion-relaxation probe plus uncertainty-aware measured arm costs\""
            << ",\"oracle_definition\":\"min(always_incremental,always_full); full wins ties\""
            << ",\"root\":" << root64
            << ",\"vertices\":" << vertices
            << ",\"batch_size\":" << batch_size
            << ",\"batches\":" << batches
            << ",\"research_claim\":false"
            << ",\"policies\":[";

  for (std::size_t p = 0; p < results.size(); ++p) {
    const auto& r = results[p];
    if (p) std::cout << ',';
    all_exact = all_exact && r.exact;
    const double total = std::accumulate(r.batch_us.begin(), r.batch_us.end(), 0.0);
    const double decision_total = std::accumulate(r.decision_us.begin(), r.decision_us.end(), 0.0);
    std::cout << "{\"name\":\"" << r.name << "\""
              << ",\"exact\":" << (r.exact ? "true" : "false")
              << ",\"total_us\":" << total
              << ",\"mean_batch_us\":" << total / std::max<std::size_t>(1, batches)
              << ",\"selector_setup_us\":" << r.selector_setup_us
              << ",\"mean_decision_us\":" << decision_total / std::max<std::size_t>(1, batches)
              << ",\"full_recompute_batches\":" << r.full_recompute_batches
              << ",\"internal_fallback_batches\":" << r.internal_fallback_batches
              << ",\"affected_vertices\":" << r.affected_vertices
              << ",\"batch_us\":";
    print_array(r.batch_us);
    std::cout << ",\"execution_us\":";
    print_array(r.execution_us);
    std::cout << ",\"decision_us\":";
    print_array(r.decision_us);
    std::cout << ",\"explicit_full\":";
    print_probe_bool_array(r.explicit_full);
    std::cout << ",\"internal_full_fallback\":";
    print_probe_bool_array(r.internal_full_fallback);
    std::cout << ",\"trace\":";
    print_probe_traces(r.traces);
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_probe_bool_array(oracle_full);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"probe_budget_definition\":\"min(10% vertices, 8x update operations), floor 64\""
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
