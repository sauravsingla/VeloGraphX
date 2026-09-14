// Publication BFS selector v3: structural-risk-aware pre-repair selection.
//
// This is a new development harness. Historical v1/v2 files and retained
// evidence are never rewritten. V3 uses only cheap pre-repair signals from the
// current exact BFS state plus measured arm costs. Hosted timings remain
// engineering evidence only (research_claim=false).
#define main velographx_adaptive_policy_helpers_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>
#include <unordered_map>

namespace {

struct StructuralSignals {
  double parent_candidate_fraction{0.0};
  double deletion_tail_risk{0.0};
  double relaxable_insertion_fraction{0.0};
  double insertion_gain_risk{0.0};
  double reachable_endpoint_fraction{0.0};
  double mean_touched_depth_fraction{0.0};
  std::size_t parent_candidates{0};
};

struct V3Trace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double previous_affected_fraction{0.0};
  double parent_candidate_fraction{0.0};
  double deletion_tail_risk{0.0};
  double relaxable_insertion_fraction{0.0};
  double insertion_gain_risk{0.0};
  double reachable_endpoint_fraction{0.0};
  double mean_touched_depth_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  bool internal_full_fallback{false};
  std::string reason;
};

struct V3Result {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<double> execution_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_full_fallback;
  std::vector<V3Trace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

struct DepthProfile {
  std::vector<double> suffix_fraction;
  std::uint32_t max_depth{0};
  std::size_t reachable{0};
};

DepthProfile make_depth_profile(const std::vector<std::uint32_t>& dist) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  DepthProfile p;
  for (auto d : dist) {
    if (d == unreachable) continue;
    ++p.reachable;
    p.max_depth = std::max(p.max_depth, d);
  }
  std::vector<std::size_t> counts(static_cast<std::size_t>(p.max_depth) + 1, 0);
  for (auto d : dist) {
    if (d != unreachable) ++counts[d];
  }
  p.suffix_fraction.assign(counts.size(), 0.0);
  std::size_t suffix = 0;
  for (std::size_t i = counts.size(); i-- > 0;) {
    suffix += counts[i];
    p.suffix_fraction[i] = static_cast<double>(suffix) /
        static_cast<double>(std::max<std::size_t>(1, p.reachable));
  }
  return p;
}

StructuralSignals structural_signals(const velographx::DynamicGraph& graph,
                                     const velographx::UpdateBatch& updates,
                                     const std::vector<std::uint32_t>& dist,
                                     const DepthProfile& profile) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  StructuralSignals s;
  std::unordered_map<velographx::VertexId, std::size_t> lost_shortest;
  lost_shortest.reserve(updates.updates.size());

  std::size_t reachable_endpoints = 0;
  std::size_t finite_depth_endpoints = 0;
  double depth_sum = 0.0;
  std::size_t relaxable_insertions = 0;
  double max_insertion_gain = 0.0;

  for (const auto& update : updates.updates) {
    const auto u = update.src;
    const auto v = update.dst;
    if (u < dist.size() && dist[u] != unreachable) {
      ++reachable_endpoints;
      ++finite_depth_endpoints;
      depth_sum += profile.max_depth == 0 ? 0.0 :
          static_cast<double>(dist[u]) / static_cast<double>(profile.max_depth);
    }
    if (v < dist.size() && dist[v] != unreachable) {
      ++reachable_endpoints;
      ++finite_depth_endpoints;
      depth_sum += profile.max_depth == 0 ? 0.0 :
          static_cast<double>(dist[v]) / static_cast<double>(profile.max_depth);
    }

    if (!update.add) {
      if (u < dist.size() && v < dist.size() &&
          dist[u] != unreachable && dist[v] != unreachable &&
          dist[u] + 1 == dist[v] && velographx::has_edge(graph, u, v)) {
        ++lost_shortest[v];
      }
      continue;
    }

    if (u >= dist.size() || v >= dist.size() || dist[u] == unreachable) continue;
    const auto candidate = dist[u] + 1;
    if (dist[v] == unreachable || candidate < dist[v]) {
      ++relaxable_insertions;
      double gain = 1.0;
      if (dist[v] != unreachable && profile.max_depth != 0) {
        gain = std::min(1.0,
            static_cast<double>(dist[v] - candidate) /
            static_cast<double>(profile.max_depth));
      }
      max_insertion_gain = std::max(max_insertion_gain, gain);
    }
  }

  double max_tail = 0.0;
  for (const auto& [v, lost] : lost_shortest) {
    if (v >= dist.size() || dist[v] == unreachable) continue;
    std::size_t support = 0;
    velographx::for_each_in_neighbor(graph, v, [&](velographx::VertexId p) {
      if (p < dist.size() && dist[p] != unreachable && dist[p] + 1 == dist[v]) {
        ++support;
      }
    });
    if (support == 0 || lost < support) continue;
    ++s.parent_candidates;
    const auto d = static_cast<std::size_t>(dist[v]);
    if (d < profile.suffix_fraction.size()) {
      max_tail = std::max(max_tail, profile.suffix_fraction[d]);
    }
  }

  const auto operations = std::max<std::size_t>(1, updates.updates.size());
  s.parent_candidate_fraction = static_cast<double>(s.parent_candidates) /
      static_cast<double>(operations);
  s.deletion_tail_risk = max_tail;
  s.relaxable_insertion_fraction = static_cast<double>(relaxable_insertions) /
      static_cast<double>(operations);
  s.insertion_gain_risk = max_insertion_gain;
  s.reachable_endpoint_fraction = static_cast<double>(reachable_endpoints) /
      static_cast<double>(operations * 2);
  s.mean_touched_depth_fraction = finite_depth_endpoints == 0 ? 0.0 :
      depth_sum / static_cast<double>(finite_depth_endpoints);
  return s;
}

V3Result run_baseline(const std::string& policy,
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
  V3Result r;
  r.name = policy;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();
    const double uf = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    bool full = policy == "always_full" ||
        (policy == "simple_threshold" && uf >= simple_update_fraction);
    const auto decision_begin = Clock::now();
    const auto decision_end = Clock::now();
    r.decision_us.push_back(
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count());
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
    r.execution_us.push_back(
        std::chrono::duration<double, std::micro>(exec_end - exec_begin).count());
    r.batch_us.push_back(
        std::chrono::duration<double, std::micro>(exec_end - total_begin).count());
    r.explicit_full.push_back(full);
    r.internal_full_fallback.push_back(fallback);
    V3Trace t;
    t.update_fraction = uf;
    t.explicit_full = full;
    t.internal_full_fallback = fallback;
    t.reason = policy;
    r.traces.push_back(t);
    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
  }
  return r;
}

V3Result run_adaptive_v3(const std::vector<Edge>& edges,
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
  const double initial_full_us =
      std::chrono::duration<double, std::micro>(init_end - init_begin).count();

  V3Result r;
  r.name = "adaptive";
  const auto setup_begin = Clock::now();
  const auto profile = make_depth_profile(bfs.distances());
  const double reachable_fraction = static_cast<double>(profile.reachable) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  r.selector_setup_us =
      std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();

  double ema_inc = 0.0;
  double ema_full = initial_full_us;
  double ema_inc_error = 0.0;
  double ema_full_error = 0.0;
  double last_inc_update_fraction = 0.0;
  double previous_affected_fraction = 0.0;
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

    V3Trace t;
    t.reachable_fraction = reachable_fraction;
    t.previous_affected_fraction = previous_affected_fraction;
    t.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));

    const auto decision_begin = Clock::now();
    const auto signals = structural_signals(graph, updates, bfs.distances(), profile);
    t.parent_candidate_fraction = signals.parent_candidate_fraction;
    t.deletion_tail_risk = signals.deletion_tail_risk;
    t.relaxable_insertion_fraction = signals.relaxable_insertion_fraction;
    t.insertion_gain_risk = signals.insertion_gain_risk;
    t.reachable_endpoint_fraction = signals.reachable_endpoint_fraction;
    t.mean_touched_depth_fraction = signals.mean_touched_depth_fraction;

    // A deletion candidate whose BFS depth leaves more than the engine's normal
    // affected-work budget behind it is a semantic warning that local repair can
    // cease to be local. This uses the same 5% affected-work scale already used
    // by the engine, not a batch-size threshold.
    const bool deletion_budget_risk =
        signals.parent_candidates > 0 && signals.deletion_tail_risk > kAffectedBudget;
    const bool insertion_expansion_risk =
        signals.relaxable_insertion_fraction > 0.0 &&
        signals.insertion_gain_risk >= 0.50 && reachable_fraction < 0.95;

    bool choose_full = false;
    if (!have_inc) {
      choose_full = deletion_budget_risk || insertion_expansion_risk;
      t.reason = choose_full ? "v3_structural_cold_full" : "v3_incremental_probe";
    } else {
      const double ratio = (t.update_fraction + 1e-12) /
          (last_inc_update_fraction + 1e-12);
      const double scale = std::clamp(std::sqrt(ratio), 0.35, 3.50);
      const double structural_multiplier = 1.0 +
          3.0 * std::clamp(previous_affected_fraction, 0.0, 1.0) +
          3.0 * std::clamp(signals.deletion_tail_risk, 0.0, 1.0) +
          2.0 * std::clamp(signals.parent_candidate_fraction * 16.0, 0.0, 1.0) +
          2.0 * std::clamp(signals.relaxable_insertion_fraction * 16.0, 0.0, 1.0) +
          1.5 * std::clamp(signals.insertion_gain_risk, 0.0, 1.0);
      t.predicted_incremental_us = ema_inc * scale * structural_multiplier;
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

      choose_full = deletion_budget_risk || insertion_expansion_risk || cost_separated_full;
      if (deletion_budget_risk) t.reason = "v3_deletion_budget_risk";
      else if (insertion_expansion_risk) t.reason = "v3_insertion_expansion_risk";
      else if (cost_separated_full) t.reason = "v3_cost_separated_full";
      else t.reason = "v3_incremental";
    }
    t.explicit_full = choose_full;
    const auto decision_end = Clock::now();
    r.decision_us.push_back(
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count());

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
    const double execution_us =
        std::chrono::duration<double, std::micro>(exec_end - exec_begin).count();
    double batch_us =
        std::chrono::duration<double, std::micro>(exec_end - total_begin).count();
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
        ema_full_error = have_full_error
            ? (1.0 - kEmaAlpha) * ema_full_error + kEmaAlpha * rel : rel;
        have_full_error = true;
      } else {
        const double rel = std::abs(execution_us - t.predicted_incremental_us) /
            std::max(1.0, t.predicted_incremental_us);
        ema_inc_error = have_inc_error
            ? (1.0 - kEmaAlpha) * ema_inc_error + kEmaAlpha * rel : rel;
        have_inc_error = true;
      }
    }

    if (observed_full) {
      ema_full = (1.0 - kEmaAlpha) * ema_full + kEmaAlpha * execution_us;
      full_age = 0;
      previous_affected_fraction = 0.0;
    } else {
      ema_inc = have_inc ? (1.0 - kEmaAlpha) * ema_inc + kEmaAlpha * execution_us
                         : execution_us;
      have_inc = true;
      inc_age = 0;
      last_inc_update_fraction = t.update_fraction;
      previous_affected_fraction = static_cast<double>(bfs.last_affected_vertices()) /
          static_cast<double>(std::max<std::size_t>(1, vertices));
    }

    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
    first_batch = false;
  }
  return r;
}

void print_bools(const std::vector<bool>& v) {
  std::cout << '[';
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (v[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_traces(const std::vector<V3Trace>& traces) {
  std::cout << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& t = traces[i];
    std::cout << "{\"update_fraction\":" << t.update_fraction
              << ",\"reachable_fraction\":" << t.reachable_fraction
              << ",\"previous_affected_fraction\":" << t.previous_affected_fraction
              << ",\"parent_candidate_fraction\":" << t.parent_candidate_fraction
              << ",\"deletion_tail_risk\":" << t.deletion_tail_risk
              << ",\"relaxable_insertion_fraction\":" << t.relaxable_insertion_fraction
              << ",\"insertion_gain_risk\":" << t.insertion_gain_risk
              << ",\"reachable_endpoint_fraction\":" << t.reachable_endpoint_fraction
              << ",\"mean_touched_depth_fraction\":" << t.mean_touched_depth_fraction
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

  std::vector<V3Result> results;
  results.push_back(run_baseline("always_incremental", edges, vertices, root,
                                 imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_baseline("always_full", edges, vertices, root,
                                 imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_baseline("simple_threshold", edges, vertices, root,
                                 imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_adaptive_v3(edges, vertices, root, imported_edges, batch_size));

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":4"
            << ",\"artifact_type\":\"velographx-publication-repair-recompute-policy\""
            << ",\"selector\":\"publication-preflight-bfs-v3\""
            << ",\"parent_selector\":\"publication-preflight-v2\""
            << ",\"selector_change\":\"add pre-repair shortest-parent-loss, depth-tail, and relaxable-insertion structural risk to uncertainty-aware arm-cost selection\""
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
    print_bools(r.explicit_full);
    std::cout << ",\"internal_full_fallback\":";
    print_bools(r.internal_full_fallback);
    std::cout << ",\"trace\":";
    print_traces(r.traces);
    std::cout << '}';
  }
  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_bools(oracle_full);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
