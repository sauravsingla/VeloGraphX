// Publication selector v2 for exact triangle repair-vs-recompute decisions.
//
// The retained v1 harness is included intact. V2 removes the universal 5%
// forced-full rule exposed by the preregistered 1024-edge failure regime and
// instead requires measured cost separation before selecting full recomputation.
#define main velographx_publication_policy_triangles_v1_main
#include "publication_policy_triangles.cpp"
#undef main

namespace {

Result run_adaptive_triangle_v2(const std::vector<Edge>& edges,
                                std::size_t vertices,
                                std::size_t imported_edges,
                                std::size_t batch_size) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, false);
  graph.bulk_load_edges(initial);

  const auto initial_begin = Clock::now();
  velographx::IncrementalTriangleCount triangles(graph);
  const auto initial_end = Clock::now();
  const double initial_full_us =
      std::chrono::duration<double, std::micro>(initial_end - initial_begin).count();

  Result result;
  result.name = "adaptive";

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

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    Trace t;
    const double logical_edges =
        std::max<double>(1.0, static_cast<double>(graph.edge_count_directed()) / 2.0);
    t.update_fraction = static_cast<double>(updates.updates.size()) / logical_edges;

    bool choose_full = false;
    const auto decision_begin = Clock::now();
    if (!have_incremental) {
      // The initial construction already measures full recomputation. Probe the
      // missing incremental arm first; do not infer that a 5% or any other fixed
      // update fraction makes full recomputation cheaper across algorithms.
      t.reason = "v2_incremental_probe";
    } else {
      const double ratio =
          (t.update_fraction + 1e-12) /
          (last_incremental_update_fraction + 1e-12);
      const double scale = std::clamp(std::sqrt(ratio), 0.40, 3.00);
      t.predicted_incremental_us = ema_incremental_us * scale;
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
      choose_full = inc_lower > full_upper;
      t.reason = choose_full
          ? "v2_cost_separated_full"
          : "v2_uncertain_or_incremental";
    }
    t.explicit_full = choose_full;
    const auto decision_end = Clock::now();
    const double decision_us =
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count();

    const auto execution_begin = Clock::now();
    if (choose_full) {
      graph.apply(updates);
      triangles.recompute();
      ++result.full_recompute_batches;
    } else {
      triangles.apply(updates);
    }
    const auto execution_end = Clock::now();
    const double execution_us =
        std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();
    const double batch_us =
        std::chrono::duration<double, std::micro>(execution_end - total_begin).count();

    const auto produced = triangles.value();
    triangles.recompute();
    if (produced != triangles.value()) result.exact = false;

    ++incremental_age;
    ++full_age;
    if (t.predicted_incremental_us > 0.0 && t.predicted_full_us > 0.0) {
      if (choose_full) {
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

    if (choose_full) {
      ema_full_us = (1.0 - kEmaAlpha) * ema_full_us + kEmaAlpha * execution_us;
      full_age = 0;
    } else {
      ema_incremental_us = have_incremental
          ? (1.0 - kEmaAlpha) * ema_incremental_us + kEmaAlpha * execution_us
          : execution_us;
      have_incremental = true;
      incremental_age = 0;
      last_incremental_update_fraction = t.update_fraction;
    }

    result.batch_us.push_back(batch_us);
    result.decision_us.push_back(decision_us);
    result.explicit_full.push_back(choose_full);
    result.trace.push_back(t);
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: publication_policy_triangles_v2 <edge-list> <import-rate> <batch-size> <simple-threshold>\n";
    return 2;
  }
  const std::string path = argv[1];
  const double imported_rate = std::stod(argv[2]);
  const auto batch_size = static_cast<std::size_t>(std::stoull(argv[3]));
  const double simple_threshold = std::stod(argv[4]);

  std::size_t vertices = 0;
  const auto edges = read_edges(path, vertices);
  const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
  if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size() ||
      batch_size == 0) return 2;

  const std::vector<std::string> baseline_names = {
      "always_incremental", "always_full", "simple_threshold", "history_cost_model"};
  std::vector<Result> results;
  for (const auto& name : baseline_names) {
    results.push_back(run_policy(name, edges, vertices, imported_edges,
                                 batch_size, simple_threshold));
  }
  results.push_back(run_adaptive_triangle_v2(edges, vertices, imported_edges, batch_size));

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":2"
            << ",\"artifact_type\":\"velographx-cross-algorithm-repair-recompute-policy\""
            << ",\"algorithm\":\"exact_triangle_count\""
            << ",\"selector\":\"publication-preflight-triangle-v2\""
            << ",\"parent_selector\":\"publication-preflight-triangle-v1\""
            << ",\"selector_change\":\"remove fixed update-fraction forced-full rule; require uncertainty-separated measured cost evidence\""
            << ",\"oracle_definition\":\"min(always_incremental,always_full); full wins ties\""
            << ",\"input_order_preserved\":true"
            << ",\"vertices\":" << vertices
            << ",\"logical_edges\":" << edges.size()
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
              << ",\"mean_batch_us\":" << total / std::max<std::size_t>(1, batches)
              << ",\"mean_decision_us\":"
              << decision_total / std::max<std::size_t>(1, batches)
              << ",\"full_recompute_batches\":" << r.full_recompute_batches
              << ",\"batch_us\":";
    print_double_array(r.batch_us);
    std::cout << ",\"decision_us\":";
    print_double_array(r.decision_us);
    std::cout << ",\"explicit_full\":";
    print_bool_array(r.explicit_full);
    std::cout << ",\"internal_full_fallback\":[";
    for (std::size_t i = 0; i < batches; ++i) {
      if (i) std::cout << ',';
      std::cout << "false";
    }
    std::cout << "]}";
  }

  std::cout << "],\"oracle_batch_us\":";
  print_double_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_bool_array(oracle_full);
  std::cout << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
