// Publication selector v2 for exact BFS repair-vs-recompute decisions.
//
// This file intentionally includes the v1 publication harness rather than
// rewriting it.  The PR #71/#72 selector and retained evidence therefore stay
// reproducible.  V2 is a new development line motivated by the retained
// post-PR71 failure analysis; any generalization claim requires a fresh holdout.
#define main velographx_publication_policy_bfs_v1_main
#include "publication_policy_bfs.cpp"
#undef main

namespace {

PublicationResult run_adaptive_v2(
    const std::vector<Edge>& edges,
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

  PublicationResult result;
  result.name = "adaptive";

  const auto setup_begin = Clock::now();
  const double reachable_fraction =
      static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  result.selector_setup_us =
      std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();

  // V2 removes topology-specific forced-full rules.  The retained road-family
  // result showed that sparse/high-diameter structure can make such rules choose
  // the wrong arm even while exactness is preserved.  Instead, full execution is
  // selected only when the observed online cost model separates the two arms by
  // more than their uncertainty bands.
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

    PublicationTrace trace;
    trace.reachable_fraction = reachable_fraction;
    trace.previous_affected_fraction = previous_affected_fraction;
    trace.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    trace.shallow_parent_deletion_fraction = first_batch
        ? shallow_parent_deletion_fraction(updates, bfs.distances(), graph.directed())
        : 0.0;

    bool choose_full = false;
    const auto decision_begin = Clock::now();

    if (!have_incremental) {
      // Initial BFS already supplied a real full-arm observation.  Probe the
      // missing incremental arm once instead of applying a topology or batch-size
      // threshold before any incremental cost has been observed.
      trace.reason = "v2_incremental_probe";
    } else {
      const double ratio =
          (trace.update_fraction + 1e-12) /
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

      const double inc_lower =
          trace.predicted_incremental_us * (1.0 - inc_uncertainty);
      const double full_upper = trace.predicted_full_us * (1.0 + full_uncertainty);
      choose_full = inc_lower > full_upper;
      trace.reason = choose_full
          ? "v2_cost_separated_full"
          : "v2_uncertain_or_incremental";
    }

    trace.explicit_full = choose_full;
    const auto decision_end = Clock::now();
    const double decision_us =
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count();
    result.decision_us.push_back(decision_us);

    const auto execution_begin = Clock::now();
    bool internal_fallback = false;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++result.full_recompute_batches;
    } else {
      bfs.apply(updates);
      result.affected_vertices += bfs.last_affected_vertices();
      internal_fallback = bfs.last_used_full_recompute();
      if (internal_fallback) {
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

    trace.internal_full_fallback = internal_fallback;
    result.batch_us.push_back(batch_us);
    result.execution_us.push_back(execution_us);
    result.explicit_full.push_back(choose_full);
    result.internal_full_fallback.push_back(internal_fallback);
    result.fallback_total_us.push_back(internal_fallback ? execution_us : 0.0);
    result.traces.push_back(trace);

    ++incremental_age;
    ++full_age;
    const bool observed_full = choose_full || internal_fallback;
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

    const auto reference = full_bfs(graph, root);
    if (reference != bfs.distances()) result.exact = false;
    first_batch = false;
  }

  return result;
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

  const std::vector<std::string> baseline_names = {
      "always_incremental", "always_full", "simple_threshold", "history_cost_model"};
  std::vector<PublicationResult> results;
  for (const auto& name : baseline_names) {
    results.push_back(run_publication_policy(
        name, edges, vertices, root, imported_edges, batch_size,
        simple_update_fraction));
  }
  results.push_back(run_adaptive_v2(edges, vertices, root, imported_edges, batch_size));

  const auto batches = results.front().batch_us.size();
  const auto& incremental = results[0];
  const auto& full = results[1];
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(incremental.batch_us[i] < full.batch_us[i]);
    oracle[i] = oracle_full[i] ? full.batch_us[i] : incremental.batch_us[i];
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
    const auto& result = results[p];
    if (p) std::cout << ',';
    all_exact = all_exact && result.exact;
    const double total_us =
        std::accumulate(result.batch_us.begin(), result.batch_us.end(), 0.0);
    const double total_decision_us =
        std::accumulate(result.decision_us.begin(), result.decision_us.end(), 0.0);
    std::cout << "{\"name\":\"" << result.name << "\""
              << ",\"exact\":" << (result.exact ? "true" : "false")
              << ",\"total_us\":" << total_us
              << ",\"mean_batch_us\":"
              << total_us / std::max<std::size_t>(1, batches)
              << ",\"selector_setup_us\":" << result.selector_setup_us
              << ",\"mean_decision_us\":"
              << total_decision_us / std::max<std::size_t>(1, batches)
              << ",\"full_recompute_batches\":" << result.full_recompute_batches
              << ",\"internal_fallback_batches\":" << result.internal_fallback_batches
              << ",\"affected_vertices\":" << result.affected_vertices
              << ",\"batch_us\":";
    print_array(result.batch_us);
    std::cout << ",\"execution_us\":";
    print_array(result.execution_us);
    std::cout << ",\"decision_us\":";
    print_array(result.decision_us);
    std::cout << ",\"explicit_full\":";
    print_bool_array(result.explicit_full);
    std::cout << ",\"internal_full_fallback\":";
    print_bool_array(result.internal_full_fallback);
    std::cout << ",\"fallback_total_us\":";
    print_array(result.fallback_total_us);
    std::cout << ",\"trace\":";
    print_publication_trace_array(result.traces);
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_bool_array(oracle_full);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
