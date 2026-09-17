// Clean feature-switch ablation for the current publication selector.
//
// The primary publication harness remains untouched. This executable freezes
// the same thresholds/state update rules and changes one selector mechanism at
// a time: structural preflight, previous affected-work inflation, or uncertainty
// gating. Exact incremental/full arms remain isolated with fallback disabled.
#define main velographx_frozen_adaptive_policy_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>

namespace {

struct AblationConfig {
  std::string name;
  bool structural{true};
  bool affected{true};
  bool uncertainty{true};
};

struct AblationTrace {
  double update_fraction{0.0};
  double reachable_fraction{0.0};
  double shallow_parent_deletion_fraction{0.0};
  double previous_affected_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  std::string reason;
};

struct AblationResult {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<bool> explicit_full;
  std::vector<AblationTrace> traces;
  double selector_setup_us{0.0};
  bool exact{true};
};

AblationResult run_policy(
    const AblationConfig& config,
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
  velographx::IncrementalBFS bfs(graph, root, 2.0);
  const auto initial_bfs_end = Clock::now();
  const double initial_bfs_us =
      std::chrono::duration<double, std::micro>(initial_bfs_end - initial_bfs_begin).count();

  AblationResult result;
  result.name = config.name;
  const auto setup_begin = Clock::now();
  const double initial_reachable_fraction =
      static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const auto setup_end = Clock::now();
  if (config.name.rfind("adaptive", 0) == 0) {
    result.selector_setup_us =
        std::chrono::duration<double, std::micro>(setup_end - setup_begin).count();
  }

  double previous_affected_fraction = 0.0;
  double ema_incremental_us = 0.0;
  double ema_full_us = initial_bfs_us;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  bool have_incremental_error = false;
  bool have_full_error = false;
  bool have_incremental = false;
  bool have_full = true;
  std::size_t incremental_age = kFreshAge + 1;
  std::size_t full_age = 0;
  const bool large_scale = vertices >= 200000;
  bool first_batch = true;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    AblationTrace trace;
    trace.reachable_fraction = initial_reachable_fraction;
    trace.previous_affected_fraction = previous_affected_fraction;
    trace.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    trace.shallow_parent_deletion_fraction = first_batch
        ? shallow_parent_deletion_fraction(updates, bfs.distances(), graph.directed())
        : 0.0;

    bool choose_full = false;
    const auto decision_begin = Clock::now();
    const bool adaptive = config.name.rfind("adaptive", 0) == 0;

    if (config.name == "always_full") {
      choose_full = true;
      trace.reason = "always_full";
    } else if (config.name == "always_incremental") {
      choose_full = false;
      trace.reason = "always_incremental";
    } else if (config.name == "simple_threshold") {
      choose_full = trace.update_fraction >= simple_update_fraction;
      trace.reason = choose_full ? "threshold_full" : "threshold_incremental";
    } else if (adaptive) {
      const double affected_multiplier = config.affected ? previous_affected_fraction : 0.0;
      if (large_scale) {
        const double normalized_scale = have_incremental
            ? std::sqrt((trace.update_fraction + 1e-12) /
                        (last_incremental_update_fraction + 1e-12))
            : 1.0;
        const double bounded_scale = std::clamp(normalized_scale, 0.50, 2.50);
        const double predicted_incremental = have_incremental
            ? ema_incremental_us * bounded_scale * (1.0 + 3.0 * affected_multiplier)
            : 0.0;
        const double predicted_full = ema_full_us;
        trace.predicted_incremental_us = predicted_incremental;
        trace.predicted_full_us = predicted_full;

        const double inc_uncertainty = have_incremental_error
            ? std::clamp(ema_incremental_rel_error, 0.05, 0.35) : 0.35;
        const double full_uncertainty = have_full_error
            ? std::clamp(ema_full_rel_error, 0.05, 0.35) : 0.20;
        const double inc_lower = predicted_incremental * (1.0 - inc_uncertainty);
        const double full_upper = predicted_full * (1.0 + full_uncertainty);
        const bool shallow_cold_start = config.structural && first_batch &&
            trace.shallow_parent_deletion_fraction > 0.0;

        if (config.structural && trace.update_fraction >= kPreflightFullUpdate) {
          choose_full = true;
          trace.reason = "large_preflight_full";
        } else if (shallow_cold_start) {
          choose_full = true;
          trace.reason = "large_shallow_cold_start";
        } else if (!have_incremental) {
          choose_full = false;
          trace.reason = "large_initial_incremental_probe";
        } else if (config.uncertainty ? (inc_lower > full_upper)
                                     : (predicted_incremental > predicted_full)) {
          choose_full = true;
          trace.reason = config.uncertainty
              ? "large_uncertainty_confident_full"
              : "large_point_estimate_full";
        } else {
          choose_full = false;
          trace.reason = config.uncertainty
              ? "large_uncertainty_overlap_incremental"
              : "large_point_estimate_incremental";
        }
      } else {
        const double predicted_incremental = have_incremental
            ? ema_incremental_us * (1.0 + 2.0 * affected_multiplier)
            : 0.0;
        const double predicted_full = have_full ? ema_full_us : 0.0;
        trace.predicted_incremental_us = predicted_incremental;
        trace.predicted_full_us = predicted_full;
        const bool fresh_model = have_incremental && have_full &&
            incremental_age <= kFreshAge && full_age <= kFreshAge;

        if (config.structural && trace.update_fraction >= kPreflightFullUpdate) {
          choose_full = true;
          trace.reason = "scale_preflight_full";
        } else if (config.structural && initial_reachable_fraction <= kVerySparseReach) {
          choose_full = true;
          trace.reason = "scale_very_sparse_reach";
        } else if (config.structural && initial_reachable_fraction < kSparseReach &&
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
      throw std::runtime_error("unknown ablation policy");
    }

    trace.explicit_full = choose_full;
    const auto decision_end = Clock::now();
    const double decision_us =
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count();

    const auto execution_begin = Clock::now();
    bool internal_fallback = false;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
    } else {
      bfs.apply(updates);
      internal_fallback = bfs.last_used_full_recompute();
    }
    const auto execution_end = Clock::now();
    double batch_us =
        std::chrono::duration<double, std::micro>(execution_end - total_begin).count();
    if (adaptive && first_batch) batch_us += result.selector_setup_us;
    result.batch_us.push_back(batch_us);
    result.decision_us.push_back(decision_us);
    result.explicit_full.push_back(choose_full);
    result.traces.push_back(trace);

    if (adaptive) {
      ++incremental_age;
      ++full_age;
      const bool observed_full = choose_full || internal_fallback;
      if (large_scale && trace.predicted_incremental_us > 0.0 &&
          trace.predicted_full_us > 0.0) {
        if (observed_full) {
          const double rel = std::abs(
              std::chrono::duration<double, std::micro>(execution_end - execution_begin).count() -
              trace.predicted_full_us) / std::max(1.0, trace.predicted_full_us);
          ema_full_rel_error = have_full_error
              ? (1.0 - kEmaAlpha) * ema_full_rel_error + kEmaAlpha * rel : rel;
          have_full_error = true;
        } else {
          const double observed_us =
              std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();
          const double rel = std::abs(observed_us - trace.predicted_incremental_us) /
              std::max(1.0, trace.predicted_incremental_us);
          ema_incremental_rel_error = have_incremental_error
              ? (1.0 - kEmaAlpha) * ema_incremental_rel_error + kEmaAlpha * rel : rel;
          have_incremental_error = true;
        }
      }
      const double execution_us =
          std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();
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

void print_bool_vector(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) return 2;
  try {
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

    const std::vector<AblationConfig> configs = {
        {"always_incremental", true, true, true},
        {"always_full", true, true, true},
        {"simple_threshold", true, true, true},
        {"adaptive", true, true, true},
        {"adaptive_no_structural", false, true, true},
        {"adaptive_no_affected", true, false, true},
        {"adaptive_no_uncertainty", true, true, false},
    };
    std::vector<AblationResult> results;
    for (const auto& config : configs) {
      results.push_back(run_policy(
          config, edges, vertices, root, imported_edges, batch_size,
          simple_update_fraction));
    }

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
    std::cout << "{\"schema_version\":1"
              << ",\"artifact_type\":\"velographx-publication-selector-feature-ablation\""
              << ",\"selector\":\"publication-preflight-v1\""
              << ",\"fallback_disabled_for_clean_arms\":true"
              << ",\"root\":" << root64
              << ",\"vertices\":" << vertices
              << ",\"batch_size\":" << batch_size
              << ",\"batches\":" << batches
              << ",\"policies\":[";
    for (std::size_t p = 0; p < results.size(); ++p) {
      if (p) std::cout << ',';
      const auto& result = results[p];
      all_exact = all_exact && result.exact;
      const double total_us =
          std::accumulate(result.batch_us.begin(), result.batch_us.end(), 0.0);
      std::cout << "{\"name\":\"" << result.name << "\""
                << ",\"exact\":" << (result.exact ? "true" : "false")
                << ",\"total_us\":" << total_us
                << ",\"selector_setup_us\":" << result.selector_setup_us
                << ",\"batch_us\":";
      print_array(result.batch_us);
      std::cout << ",\"decision_us\":";
      print_array(result.decision_us);
      std::cout << ",\"explicit_full\":";
      print_bool_vector(result.explicit_full);
      std::cout << '}';
    }
    std::cout << "],\"oracle_batch_us\":";
    print_array(oracle);
    std::cout << ",\"oracle_full\":";
    print_bool_vector(oracle_full);
    std::cout << ",\"verification_excluded_from_timing\":true"
              << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
              << "}\n";
    return all_exact ? 0 : 1;
  } catch (const std::exception& exc) {
    std::cerr << "error: " << exc.what() << '\n';
    return 2;
  }
}
