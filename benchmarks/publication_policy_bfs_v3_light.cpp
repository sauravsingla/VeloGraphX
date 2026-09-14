// Publication BFS selector v3-light: low-overhead pre-repair selection.
//
// Historical v1/v2, the first structural v3, and the bounded-probe v3 attempt
// remain untouched. This candidate keeps v2's measured-arm/uncertainty model,
// adds only O(batch) signals available from the current exact BFS state, and
// reuses the historical 5% preflight guard for cold large updates. It performs
// no propagation probe and no per-update neighbor-support scan. All selector
// work is included in adaptive latency. Hosted timings remain engineering
// evidence only (research_claim=false).
#define main velographx_adaptive_policy_helpers_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>

namespace {

struct LightSignals {
  double shortest_parent_deletion_fraction{0.0};
  double relaxable_insertion_fraction{0.0};
  double unreachable_target_insertion_fraction{0.0};
  double reachable_endpoint_fraction{0.0};
  double structural_change_fraction{0.0};
};

struct LightTrace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double previous_affected_fraction{0.0};
  double shortest_parent_deletion_fraction{0.0};
  double relaxable_insertion_fraction{0.0};
  double unreachable_target_insertion_fraction{0.0};
  double reachable_endpoint_fraction{0.0};
  double structural_change_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  bool internal_full_fallback{false};
  std::string reason;
};

struct LightResult {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<double> execution_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_full_fallback;
  std::vector<LightTrace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

LightSignals light_signals(const velographx::UpdateBatch& updates,
                           const std::vector<std::uint32_t>& dist) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  LightSignals s;
  std::size_t parent_deletions = 0;
  std::size_t relaxable_insertions = 0;
  std::size_t unreachable_target_insertions = 0;
  std::size_t reachable_endpoints = 0;

  for (const auto& update : updates.updates) {
    const auto u = update.src;
    const auto v = update.dst;
    const bool u_reachable = u < dist.size() && dist[u] != unreachable;
    const bool v_reachable = v < dist.size() && dist[v] != unreachable;
    if (u_reachable) ++reachable_endpoints;
    if (v_reachable) ++reachable_endpoints;

    if (!update.add) {
      if (u_reachable && v_reachable && dist[u] + 1 == dist[v]) ++parent_deletions;
      continue;
    }

    if (!u_reachable || v >= dist.size()) continue;
    const auto candidate = dist[u] + 1;
    if (!v_reachable || candidate < dist[v]) {
      ++relaxable_insertions;
      if (!v_reachable) ++unreachable_target_insertions;
    }
  }

  const auto operations = std::max<std::size_t>(1, updates.updates.size());
  const double denom = static_cast<double>(operations);
  s.shortest_parent_deletion_fraction = static_cast<double>(parent_deletions) / denom;
  s.relaxable_insertion_fraction = static_cast<double>(relaxable_insertions) / denom;
  s.unreachable_target_insertion_fraction =
      static_cast<double>(unreachable_target_insertions) / denom;
  s.reachable_endpoint_fraction = static_cast<double>(reachable_endpoints) /
      static_cast<double>(operations * 2);
  s.structural_change_fraction = std::min(
      1.0, s.shortest_parent_deletion_fraction + s.relaxable_insertion_fraction);
  return s;
}

LightResult run_light_baseline(const std::string& policy,
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
  LightResult r;
  r.name = policy;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();
    const double update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    const bool choose_full = policy == "always_full" ||
        (policy == "simple_threshold" && update_fraction >= simple_update_fraction);
    r.decision_us.push_back(0.0);

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
    const double execution =
        std::chrono::duration<double, std::micro>(exec_end - exec_begin).count();
    r.execution_us.push_back(execution);
    r.batch_us.push_back(
        std::chrono::duration<double, std::micro>(exec_end - total_begin).count());
    r.explicit_full.push_back(choose_full);
    r.internal_full_fallback.push_back(fallback);
    LightTrace t;
    t.update_fraction = update_fraction;
    t.explicit_full = choose_full;
    t.internal_full_fallback = fallback;
    t.reason = policy;
    r.traces.push_back(t);
    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
  }
  return r;
}

LightResult run_adaptive_light(const std::vector<Edge>& edges,
                               std::size_t vertices,
                               velographx::VertexId root,
                               std::size_t imported_edges,
                               std::size_t batch_size) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);

  const auto initial_bfs_begin = Clock::now();
  velographx::IncrementalBFS bfs(graph, root, 2.0);
  const auto initial_bfs_end = Clock::now();
  const double initial_full_us =
      std::chrono::duration<double, std::micro>(initial_bfs_end - initial_bfs_begin).count();

  LightResult r;
  r.name = "adaptive";
  const auto setup_begin = Clock::now();
  const double reachable_fraction =
      static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  r.selector_setup_us =
      std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();

  double previous_affected_fraction = 0.0;
  double ema_incremental_us = 0.0;
  double ema_full_us = initial_full_us;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  bool have_incremental = false;
  bool have_incremental_error = false;
  bool have_full_error = false;
  std::size_t incremental_age = 0;
  std::size_t full_age = 0;
  bool first_batch = true;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    LightTrace t;
    t.reachable_fraction = reachable_fraction;
    t.previous_affected_fraction = previous_affected_fraction;
    t.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));

    const auto decision_begin = Clock::now();
    const auto signals = light_signals(updates, bfs.distances());
    t.shortest_parent_deletion_fraction = signals.shortest_parent_deletion_fraction;
    t.relaxable_insertion_fraction = signals.relaxable_insertion_fraction;
    t.unreachable_target_insertion_fraction = signals.unreachable_target_insertion_fraction;
    t.reachable_endpoint_fraction = signals.reachable_endpoint_fraction;
    t.structural_change_fraction = signals.structural_change_fraction;

    bool choose_full = false;
    if (t.update_fraction >= kPreflightFullUpdate) {
      // Reuse the historical preflight guard. This is not fitted to H4.
      choose_full = true;
      t.reason = "v3_light_existing_preflight_full";
    } else if (!have_incremental) {
      // Initial BFS already measured full recomputation; calibrate the missing
      // arm once when the historical large-update guard does not fire.
      t.reason = "v3_light_incremental_probe";
    } else {
      const double ratio = (t.update_fraction + 1e-12) /
          (last_incremental_update_fraction + 1e-12);
      const double scale = std::clamp(std::sqrt(ratio), 0.40, 3.00);
      // Structural pressure is deliberately bounded and can only increase the
      // v2 prediction by at most 2x. It is evidence for expected work, never a
      // stand-alone forced-full rule.
      const double structural_multiplier = 1.0 + signals.structural_change_fraction;
      t.predicted_incremental_us = ema_incremental_us * scale *
          (1.0 + 4.0 * std::clamp(previous_affected_fraction, 0.0, 1.0)) *
          structural_multiplier;
      t.predicted_full_us = ema_full_us;

      const double inc_model_error = have_incremental_error
          ? std::clamp(ema_incremental_rel_error, 0.08, 0.50) : 0.40;
      const double full_model_error = have_full_error
          ? std::clamp(ema_full_rel_error, 0.08, 0.40) : 0.25;
      const double inc_age_penalty =
          std::min(0.25, 0.02 * static_cast<double>(incremental_age));
      const double full_age_penalty =
          std::min(0.30, 0.015 * static_cast<double>(full_age));
      const double inc_uncertainty = std::min(0.70, inc_model_error + inc_age_penalty);
      const double full_uncertainty = std::min(0.60, full_model_error + full_age_penalty);
      const double inc_lower = t.predicted_incremental_us * (1.0 - inc_uncertainty);
      const double full_upper = t.predicted_full_us * (1.0 + full_uncertainty);
      const bool confidently_full = inc_lower > full_upper;
      const bool fresh_expected_full = full_age <= kFreshAge &&
          t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;
      choose_full = confidently_full || fresh_expected_full;
      if (confidently_full) t.reason = "v3_light_confident_full";
      else if (fresh_expected_full) t.reason = "v3_light_fresh_expected_full";
      else t.reason = "v3_light_incremental";
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

    ++incremental_age;
    ++full_age;
    const bool observed_full = choose_full || fallback;
    if (t.predicted_incremental_us > 0.0 && t.predicted_full_us > 0.0) {
      if (observed_full) {
        const double rel = std::abs(execution_us - t.predicted_full_us) /
            std::max(1.0, t.predicted_full_us);
        ema_full_rel_error = have_full_error
            ? (1.0 - kEmaAlpha) * ema_full_rel_error + kEmaAlpha * rel : rel;
        have_full_error = true;
      } else {
        const double rel = std::abs(execution_us - t.predicted_incremental_us) /
            std::max(1.0, t.predicted_incremental_us);
        ema_incremental_rel_error = have_incremental_error
            ? (1.0 - kEmaAlpha) * ema_incremental_rel_error + kEmaAlpha * rel : rel;
        have_incremental_error = true;
      }
    }

    if (observed_full) {
      ema_full_us = (1.0 - kEmaAlpha) * ema_full_us + kEmaAlpha * execution_us;
      full_age = 0;
      previous_affected_fraction = 0.0;
    } else {
      ema_incremental_us = have_incremental
          ? (1.0 - kEmaAlpha) * ema_incremental_us + kEmaAlpha * execution_us
          : execution_us;
      have_incremental = true;
      incremental_age = 0;
      last_incremental_update_fraction = t.update_fraction;
      previous_affected_fraction =
          static_cast<double>(bfs.last_affected_vertices()) /
          static_cast<double>(std::max<std::size_t>(1, vertices));
    }

    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
    first_batch = false;
  }
  return r;
}

void print_light_bools(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_light_traces(const std::vector<LightTrace>& traces) {
  std::cout << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& t = traces[i];
    std::cout << "{\"update_fraction\":" << t.update_fraction
              << ",\"reachable_fraction\":" << t.reachable_fraction
              << ",\"previous_affected_fraction\":" << t.previous_affected_fraction
              << ",\"shortest_parent_deletion_fraction\":" << t.shortest_parent_deletion_fraction
              << ",\"relaxable_insertion_fraction\":" << t.relaxable_insertion_fraction
              << ",\"unreachable_target_insertion_fraction\":" << t.unreachable_target_insertion_fraction
              << ",\"reachable_endpoint_fraction\":" << t.reachable_endpoint_fraction
              << ",\"structural_change_fraction\":" << t.structural_change_fraction
              << ",\"predicted_incremental_us\":" << t.predicted_incremental_us
              << ",\"predicted_full_us\":" << t.predicted_full_us
              << ",\"explicit_full\":" << (t.explicit_full ? "true" : "false")
              << ",\"internal_full_fallback\":"
              << (t.internal_full_fallback ? "true" : "false")
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
  if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size() ||
      batch_size == 0) return 2;

  std::vector<LightResult> results;
  results.push_back(run_light_baseline("always_incremental", edges, vertices, root,
                                      imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_light_baseline("always_full", edges, vertices, root,
                                      imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_light_baseline("simple_threshold", edges, vertices, root,
                                      imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_adaptive_light(edges, vertices, root, imported_edges, batch_size));

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":6"
            << ",\"artifact_type\":\"velographx-publication-repair-recompute-policy\""
            << ",\"selector\":\"publication-preflight-bfs-v3-light\""
            << ",\"parent_selector\":\"publication-preflight-v2\""
            << ",\"selector_change\":\"low-overhead O(batch) BFS-state signals plus v2 measured-arm uncertainty; no propagation probe or hard structural forcing\""
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
    const double decision_total =
        std::accumulate(r.decision_us.begin(), r.decision_us.end(), 0.0);
    std::cout << "{\"name\":\"" << r.name << "\""
              << ",\"exact\":" << (r.exact ? "true" : "false")
              << ",\"total_us\":" << total
              << ",\"mean_batch_us\":" << total / std::max<std::size_t>(1, batches)
              << ",\"selector_setup_us\":" << r.selector_setup_us
              << ",\"mean_decision_us\":"
              << decision_total / std::max<std::size_t>(1, batches)
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
    print_light_bools(r.explicit_full);
    std::cout << ",\"internal_full_fallback\":";
    print_light_bools(r.internal_full_fallback);
    std::cout << ",\"trace\":";
    print_light_traces(r.traces);
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_light_bools(oracle_full);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
