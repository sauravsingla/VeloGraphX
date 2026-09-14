// Publication selector v2 for exact BFS repair-vs-recompute decisions.
//
// The frozen historical/publication harnesses are not edited. We reuse only the
// stable graph/update helpers from adaptive_policy_bfs.cpp and build a separate
// v2 experiment around them. Hosted timings remain engineering evidence only.
#define main velographx_adaptive_policy_helpers_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>

namespace {

struct V2Trace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double previous_affected_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  bool internal_full_fallback{false};
  std::string reason;
};

struct V2Result {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<double> execution_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_full_fallback;
  std::vector<V2Trace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

V2Result run_baseline_v2(const std::string& policy,
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

  V2Result result;
  result.name = policy;
  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    const double update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    bool choose_full = policy == "always_full";
    if (policy == "simple_threshold") {
      choose_full = update_fraction >= simple_update_fraction;
    }

    const auto decision_begin = Clock::now();
    const auto decision_end = Clock::now();
    result.decision_us.push_back(
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count());

    const auto execution_begin = Clock::now();
    bool fallback = false;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++result.full_recompute_batches;
    } else {
      bfs.apply(updates);
      result.affected_vertices += bfs.last_affected_vertices();
      fallback = bfs.last_used_full_recompute();
      if (fallback) {
        ++result.full_recompute_batches;
        ++result.internal_fallback_batches;
      }
    }
    const auto execution_end = Clock::now();
    result.execution_us.push_back(
        std::chrono::duration<double, std::micro>(execution_end - execution_begin).count());
    result.batch_us.push_back(
        std::chrono::duration<double, std::micro>(execution_end - total_begin).count());
    result.explicit_full.push_back(choose_full);
    result.internal_full_fallback.push_back(fallback);

    V2Trace t;
    t.update_fraction = update_fraction;
    t.explicit_full = choose_full;
    t.internal_full_fallback = fallback;
    t.reason = policy;
    result.traces.push_back(t);

    if (full_bfs(graph, root) != bfs.distances()) result.exact = false;
  }
  return result;
}

V2Result run_adaptive_v2(const std::vector<Edge>& edges,
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

  V2Result result;
  result.name = "adaptive";
  const auto setup_begin = Clock::now();
  const double reachable_fraction =
      static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  result.selector_setup_us =
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

    V2Trace trace;
    trace.reachable_fraction = reachable_fraction;
    trace.previous_affected_fraction = previous_affected_fraction;
    trace.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));

    bool choose_full = false;
    const auto decision_begin = Clock::now();
    if (!have_incremental) {
      // Initial construction already measured full recomputation. Probe the
      // missing arm instead of applying topology or update-size rules cold.
      trace.reason = "v2_incremental_probe";
    } else {
      const double ratio = (trace.update_fraction + 1e-12) /
          (last_incremental_update_fraction + 1e-12);
      const double scale = std::clamp(std::sqrt(ratio), 0.40, 3.00);
      trace.predicted_incremental_us = ema_incremental_us * scale *
          (1.0 + 4.0 * std::clamp(previous_affected_fraction, 0.0, 1.0));
      trace.predicted_full_us = ema_full_us;

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
      const double inc_lower = trace.predicted_incremental_us * (1.0 - inc_uncertainty);
      const double full_upper = trace.predicted_full_us * (1.0 + full_uncertainty);
      choose_full = inc_lower > full_upper;
      trace.reason = choose_full ? "v2_cost_separated_full"
                                 : "v2_uncertain_or_incremental";
    }
    trace.explicit_full = choose_full;
    const auto decision_end = Clock::now();
    result.decision_us.push_back(
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count());

    const auto execution_begin = Clock::now();
    bool fallback = false;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++result.full_recompute_batches;
    } else {
      bfs.apply(updates);
      result.affected_vertices += bfs.last_affected_vertices();
      fallback = bfs.last_used_full_recompute();
      if (fallback) {
        ++result.full_recompute_batches;
        ++result.internal_fallback_batches;
      }
    }
    const auto execution_end = Clock::now();
    const double execution_us =
        std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();
    double batch_us =
        std::chrono::duration<double, std::micro>(execution_end - total_begin).count();
    if (first_batch) batch_us += result.selector_setup_us;

    trace.internal_full_fallback = fallback;
    result.execution_us.push_back(execution_us);
    result.batch_us.push_back(batch_us);
    result.explicit_full.push_back(choose_full);
    result.internal_full_fallback.push_back(fallback);
    result.traces.push_back(trace);

    ++incremental_age;
    ++full_age;
    const bool observed_full = choose_full || fallback;
    if (trace.predicted_incremental_us > 0.0 && trace.predicted_full_us > 0.0) {
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
      ema_full_us = (1.0 - kEmaAlpha) * ema_full_us + kEmaAlpha * execution_us;
      full_age = 0;
      previous_affected_fraction = 0.0;
    } else {
      ema_incremental_us = have_incremental
          ? (1.0 - kEmaAlpha) * ema_incremental_us + kEmaAlpha * execution_us
          : execution_us;
      have_incremental = true;
      incremental_age = 0;
      last_incremental_update_fraction = trace.update_fraction;
      previous_affected_fraction =
          static_cast<double>(bfs.last_affected_vertices()) /
          static_cast<double>(std::max<std::size_t>(1, vertices));
    }

    if (full_bfs(graph, root) != bfs.distances()) result.exact = false;
    first_batch = false;
  }
  return result;
}

void print_bool_array_v2(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_trace_v2(const std::vector<V2Trace>& traces) {
  std::cout << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& t = traces[i];
    std::cout << "{\"update_fraction\":" << t.update_fraction
              << ",\"reachable_fraction\":" << t.reachable_fraction
              << ",\"previous_affected_fraction\":" << t.previous_affected_fraction
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

  std::vector<V2Result> results;
  results.push_back(run_baseline_v2("always_incremental", edges, vertices, root,
                                    imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_baseline_v2("always_full", edges, vertices, root,
                                    imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_baseline_v2("simple_threshold", edges, vertices, root,
                                    imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_adaptive_v2(edges, vertices, root, imported_edges, batch_size));

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":3"
            << ",\"artifact_type\":\"velographx-publication-repair-recompute-policy\""
            << ",\"selector\":\"publication-preflight-v2\""
            << ",\"parent_selector\":\"publication-preflight-v1\""
            << ",\"selector_change\":\"remove topology/batch forced-full rules; select full only under uncertainty-separated online cost evidence\""
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
    print_bool_array_v2(r.explicit_full);
    std::cout << ",\"internal_full_fallback\":";
    print_bool_array_v2(r.internal_full_fallback);
    std::cout << ",\"trace\":";
    print_trace_v2(r.traces);
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_bool_array_v2(oracle_full);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
