// Publication-only repair-vs-recompute harness.
//
// The historical adaptive_policy_bfs.cpp is intentionally included rather than
// edited so its frozen selector/results remain reproducible at their original
// commit. This harness layers publication telemetry and the next selector
// revision on top of the same graph/update helpers.
#define main velographx_frozen_adaptive_policy_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>

namespace {

constexpr std::size_t kHistoryMinIncrementalSamples = 3;
constexpr std::size_t kHistoryMinFullSamples = 2;

struct PublicationTrace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double shallow_parent_deletion_fraction{0.0};
  double previous_affected_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  bool internal_full_fallback{false};
  std::string reason;
};

struct PublicationResult {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<double> execution_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_full_fallback;
  std::vector<double> fallback_total_us;
  std::vector<PublicationTrace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

PublicationResult run_publication_policy(
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

  const auto initial_bfs_begin = Clock::now();
  // Keep the incremental arm permissive so policy decisions, rather than a
  // small affected-work budget, own recomputation in this publication harness.
  velographx::IncrementalBFS bfs(graph, root, 2.0);
  const auto initial_bfs_end = Clock::now();
  const double initial_bfs_us =
      std::chrono::duration<double, std::micro>(initial_bfs_end - initial_bfs_begin).count();

  PublicationResult result;
  result.name = policy;

  const auto setup_begin = Clock::now();
  const double initial_reachable_fraction =
      static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  if (policy == "adaptive") {
    result.selector_setup_us =
        std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();
  }

  // Adaptive selector state.
  double previous_affected_fraction = 0.0;
  double ema_incremental_us = 0.0;
  double ema_full_us = initial_bfs_us;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  bool have_incremental_error = false;
  bool have_full_error = false;
  bool have_incremental = false;
  bool have_full = true;  // initial BFS is already a measured full-cost baseline
  std::size_t incremental_age = kFreshAge + 1;
  std::size_t full_age = 0;
  const bool large_scale = vertices >= 200000;
  bool first_batch = true;

  // History-cost reconstruction inspired by Bok et al. 2022.
  //
  // Their model predicts incremental cost from historical affected vertices per
  // update (NRV), detection cost per affected vertex (SDC), and processing cost
  // per affected vertex (SPC), while static cost scales with graph size. The
  // VeloGraphX harness cannot separately time detection and processing inside
  // IncrementalBFS without perturbing the implementation, so this reconstruction
  // uses their combined observed cost per affected vertex as SDC+SPC. It uses no
  // VeloGraphX root/scale/uncertainty features. Calibration probes are online and
  // remain inside measured answer-ready latency.
  double history_sum_affected_per_update = 0.0;
  double history_incremental_total_us = 0.0;
  double history_incremental_total_affected = 0.0;
  double history_full_total_us = initial_bfs_us;
  std::size_t history_incremental_samples = 0;
  std::size_t history_full_samples = 1;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    PublicationTrace trace;
    trace.reachable_fraction = initial_reachable_fraction;
    trace.previous_affected_fraction = previous_affected_fraction;
    trace.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    trace.shallow_parent_deletion_fraction = first_batch
        ? shallow_parent_deletion_fraction(updates, bfs.distances(), graph.directed())
        : 0.0;

    bool choose_full = false;
    const auto decision_begin = Clock::now();

    if (policy == "always_full") {
      choose_full = true;
      trace.reason = "always_full";
    } else if (policy == "always_incremental") {
      choose_full = false;
      trace.reason = "always_incremental";
    } else if (policy == "simple_threshold") {
      choose_full = trace.update_fraction >= simple_update_fraction;
      trace.reason = choose_full ? "threshold_full" : "threshold_incremental";
    } else if (policy == "history_cost_model") {
      const double update_count =
          static_cast<double>(std::max<std::size_t>(1, updates.updates.size()));
      if (history_incremental_samples >= kHistoryMinIncrementalSamples) {
        const double nrv = history_sum_affected_per_update /
            static_cast<double>(history_incremental_samples);
        const double combined_cost_per_affected = history_incremental_total_us /
            std::max(1.0, history_incremental_total_affected);
        trace.predicted_incremental_us =
            nrv * update_count * combined_cost_per_affected;
      }
      trace.predicted_full_us = history_full_total_us /
          static_cast<double>(std::max<std::size_t>(1, history_full_samples));

      if (history_incremental_samples < kHistoryMinIncrementalSamples) {
        choose_full = false;
        trace.reason = "history_calibrate_incremental";
      } else if (history_full_samples < kHistoryMinFullSamples) {
        choose_full = true;
        trace.reason = "history_calibrate_full";
      } else {
        choose_full = trace.predicted_full_us < trace.predicted_incremental_us;
        trace.reason = choose_full ? "history_cost_full" : "history_cost_incremental";
      }
    } else if (policy == "adaptive") {
      if (large_scale) {
        const double normalized_scale = have_incremental
            ? std::sqrt((trace.update_fraction + 1e-12) /
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

        const bool shallow_cold_start =
            first_batch && trace.shallow_parent_deletion_fraction > 0.0;
        if (trace.update_fraction >= kPreflightFullUpdate) {
          choose_full = true;
          trace.reason = "large_preflight_full";
        } else if (shallow_cold_start) {
          choose_full = true;
          trace.reason = "large_shallow_cold_start";
        } else if (!have_incremental) {
          // Tail-regret fix: initial BFS already measures the full arm. Probe
          // the missing incremental arm instead of redundantly paying full again.
          choose_full = false;
          trace.reason = "large_initial_incremental_probe";
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
        if (trace.update_fraction >= kPreflightFullUpdate) {
          choose_full = true;
          trace.reason = "scale_preflight_full";
        } else if (initial_reachable_fraction <= kVerySparseReach) {
          choose_full = true;
          trace.reason = "scale_very_sparse_reach";
        } else if (initial_reachable_fraction < kSparseReach &&
                   trace.update_fraction >= kSparseReachFullUpdate) {
          choose_full = true;
          trace.reason = "scale_sparse_reach_guard";
        } else if (fresh_model &&
                   predicted_incremental > predicted_full * kLearnedFullMargin) {
          choose_full = true;
          trace.reason = "scale_fresh_cost_model";
        } else {
          choose_full = false;
          trace.reason = fresh_model
              ? "scale_confidence_incremental"
              : "scale_insufficient_evidence_incremental";
        }
      }
    } else {
      throw std::runtime_error("unknown publication policy");
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
    if (policy == "adaptive" && first_batch) batch_us += result.selector_setup_us;

    trace.internal_full_fallback = internal_fallback;
    result.batch_us.push_back(batch_us);
    result.execution_us.push_back(execution_us);
    result.explicit_full.push_back(choose_full);
    result.internal_full_fallback.push_back(internal_fallback);
    result.fallback_total_us.push_back(internal_fallback ? execution_us : 0.0);
    result.traces.push_back(trace);

    if (policy == "history_cost_model") {
      if (choose_full) {
        history_full_total_us += execution_us;
        ++history_full_samples;
      } else {
        const double update_count =
            static_cast<double>(std::max<std::size_t>(1, updates.updates.size()));
        const double affected =
            static_cast<double>(std::max<std::size_t>(1, bfs.last_affected_vertices()));
        history_sum_affected_per_update += affected / update_count;
        history_incremental_total_us += execution_us;
        history_incremental_total_affected += affected;
        ++history_incremental_samples;
      }
    }

    if (policy == "adaptive") {
      ++incremental_age;
      ++full_age;
      const bool observed_full = choose_full || internal_fallback;
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
        previous_affected_fraction =
            static_cast<double>(bfs.last_affected_vertices()) /
            static_cast<double>(std::max<std::size_t>(1, vertices));
      }
    }

    const auto reference = full_bfs(graph, root);
    if (reference != bfs.distances()) result.exact = false;
    first_batch = false;
  }

  return result;
}

void print_bool_array(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_publication_trace_array(const std::vector<PublicationTrace>& traces) {
  std::cout << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& t = traces[i];
    std::cout << "{\"update_fraction\":" << t.update_fraction
              << ",\"reachable_fraction\":" << t.reachable_fraction
              << ",\"shallow_parent_deletion_fraction\":"
              << t.shallow_parent_deletion_fraction
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

  const std::vector<std::string> names = {
      "always_incremental", "always_full", "simple_threshold",
      "history_cost_model", "adaptive"};
  std::vector<PublicationResult> results;
  for (const auto& name : names) {
    results.push_back(run_publication_policy(
        name, edges, vertices, root, imported_edges, batch_size,
        simple_update_fraction));
  }

  const auto batches = results.front().batch_us.size();
  const auto& incremental = results[0];
  const auto& full = results[1];
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    // Deterministic tie rule: full wins ties.
    oracle_full[i] = !(incremental.batch_us[i] < full.batch_us[i]);
    oracle[i] = oracle_full[i] ? full.batch_us[i] : incremental.batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":2"
            << ",\"artifact_type\":\"velographx-publication-repair-recompute-policy\""
            << ",\"selector\":\"publication-preflight-v1\""
            << ",\"frozen_historical_selector\":\"bounded-one-sided-warmup-v5\""
            << ",\"history_baseline_disclosure\":\"NRV-style methodological reconstruction inspired by Bok et al. 2022; aggregate detection+processing cost per affected vertex; online calibration included; not authors' original implementation\""
            << ",\"oracle_definition\":\"min(always_incremental,always_full); full wins ties\""
            << ",\"root\":" << root64
            << ",\"vertices\":" << vertices
            << ",\"batch_size\":" << batch_size
            << ",\"batches\":" << batches
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
