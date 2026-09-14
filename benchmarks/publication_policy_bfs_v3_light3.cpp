// Publication BFS selector v3-light3: robust mean + tail-risk preflight selection.
//
// This development candidate leaves all earlier v3 attempts intact. It addresses
// two failure modes observed only on the already-seen road/H4 development sets:
// (1) one broad incremental-repair spike should not drag the mean cost model far
//     enough to cause a retaliatory wrong-arm full recompute on the next batch;
// (2) a localized, unexplained incremental tail spike should remain visible long
//     enough to protect a later batch even if one cheap confirmation batch follows.
//
// The mechanism is deliberately generic. Incremental EMA learning rate is reduced
// in proportion to affected work using the engine's existing kAffectedBudget.
// Positive residual tail risk is normalized by the same affected-work scale, is
// decayed with the existing kEmaAlpha, and uses kLearnedFullMargin as its only
// materiality threshold. The first material tail event gets one incremental
// confirmation probe before the tail guard is allowed to choose full. No H6 data
// is generated or consumed here. Hosted timings remain research_claim=false.
#define main velographx_adaptive_policy_helpers_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <numeric>

namespace {

struct L3Signals {
  double shortest_parent_deletion_fraction{0.0};
  double relaxable_insertion_fraction{0.0};
  double unreachable_target_insertion_fraction{0.0};
  double reachable_endpoint_fraction{0.0};
  double structural_change_fraction{0.0};
};

struct L3Trace {
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
  double tail_adjusted_incremental_us{0.0};
  double incoming_tail_ratio{1.0};
  double observed_affected_fraction{0.0};
  double affected_scale{1.0};
  double normalized_tail_ratio{1.0};
  std::size_t incremental_age{0};
  std::size_t full_age{0};
  bool tail_confirmation_due{false};
  bool tail_confirmation_batch{false};
  bool explicit_full{false};
  bool internal_full_fallback{false};
  std::string reason;
};

struct L3Result {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<double> execution_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_full_fallback;
  std::vector<L3Trace> traces;
  double selector_setup_us{0.0};
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  std::size_t affected_vertices{0};
  bool exact{true};
};

L3Signals l3_signals(const velographx::UpdateBatch& updates,
                     const std::vector<std::uint32_t>& dist) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  L3Signals s;
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

L3Result run_l3_baseline(const std::string& policy,
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
  L3Result r;
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
    L3Trace t;
    t.update_fraction = update_fraction;
    t.explicit_full = choose_full;
    t.internal_full_fallback = fallback;
    t.reason = policy;
    r.traces.push_back(t);
    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
  }
  return r;
}

L3Result run_adaptive_l3(const std::vector<Edge>& edges,
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
  const double initial_full_us =
      std::chrono::duration<double, std::micro>(initial_bfs_end - initial_bfs_begin).count();

  L3Result r;
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
  double latest_full_us = initial_full_us;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  double incremental_tail_ratio = 1.0;
  bool have_incremental = false;
  bool have_incremental_error = false;
  bool have_full_error = false;
  bool deferred_after_preflight = false;
  bool tail_confirmation_due = false;
  std::size_t incremental_age = 0;
  std::size_t full_age = 0;
  bool first_batch = true;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    L3Trace t;
    t.reachable_fraction = reachable_fraction;
    t.previous_affected_fraction = previous_affected_fraction;
    t.update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    t.incremental_age = incremental_age;
    t.full_age = full_age;
    t.incoming_tail_ratio = incremental_tail_ratio;
    t.tail_confirmation_due = tail_confirmation_due;

    const auto decision_begin = Clock::now();
    const auto signals = l3_signals(updates, bfs.distances());
    t.shortest_parent_deletion_fraction = signals.shortest_parent_deletion_fraction;
    t.relaxable_insertion_fraction = signals.relaxable_insertion_fraction;
    t.unreachable_target_insertion_fraction = signals.unreachable_target_insertion_fraction;
    t.reachable_endpoint_fraction = signals.reachable_endpoint_fraction;
    t.structural_change_fraction = signals.structural_change_fraction;

    bool choose_full = false;
    bool confirmation_batch = false;
    if (t.update_fraction >= kPreflightFullUpdate) {
      choose_full = true;
      if (!have_incremental) deferred_after_preflight = true;
      t.reason = "v3_light3_existing_preflight_full";
    } else if (!have_incremental) {
      if (deferred_after_preflight && t.update_fraction >= simple_update_fraction) {
        choose_full = true;
        t.reason = "v3_light3_defer_probe_under_pressure";
      } else {
        t.reason = "v3_light3_incremental_probe";
      }
    } else {
      const double ratio = (t.update_fraction + 1e-12) /
          (last_incremental_update_fraction + 1e-12);
      const double scale = std::clamp(std::sqrt(ratio), 0.40, 3.00);
      const double structural_multiplier = 1.0 + signals.structural_change_fraction;
      t.predicted_incremental_us = ema_incremental_us * scale *
          (1.0 + 4.0 * std::clamp(previous_affected_fraction, 0.0, 1.0)) *
          structural_multiplier;
      t.predicted_full_us = latest_full_us;
      t.tail_adjusted_incremental_us =
          t.predicted_incremental_us * incremental_tail_ratio;

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

      if (tail_confirmation_due) {
        // A material tail observation receives one measured incremental
        // confirmation before it can influence a full decision. This avoids
        // reacting to a broad but transient repair wave as a persistent regime.
        choose_full = false;
        confirmation_batch = true;
        t.reason = "v3_light3_tail_confirmation_probe";
      } else if (incremental_age >= kFreshAge) {
        choose_full = false;
        t.reason = "v3_light3_refresh_incremental";
      } else {
        const bool tail_guard = incremental_tail_ratio >= kLearnedFullMargin &&
            t.tail_adjusted_incremental_us > t.predicted_full_us;
        const bool confidently_full = inc_lower > full_upper;
        const bool fresh_expected_full = full_age < kFreshAge &&
            t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;
        choose_full = tail_guard || confidently_full || fresh_expected_full;
        if (tail_guard) t.reason = "v3_light3_tail_guard_full";
        else if (confidently_full) t.reason = "v3_light3_confident_full";
        else if (fresh_expected_full) t.reason = "v3_light3_fresh_expected_full";
        else t.reason = "v3_light3_incremental";
      }
    }
    t.tail_confirmation_batch = confirmation_batch;
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
    const bool observed_full = choose_full || fallback;
    const double observed_affected_fraction = observed_full ? 0.0 :
        static_cast<double>(bfs.last_affected_vertices()) /
        static_cast<double>(std::max<std::size_t>(1, vertices));
    t.observed_affected_fraction = observed_affected_fraction;
    t.affected_scale = 1.0 + observed_affected_fraction /
        std::max(kAffectedBudget, std::numeric_limits<double>::epsilon());

    r.execution_us.push_back(execution_us);
    r.batch_us.push_back(batch_us);
    r.explicit_full.push_back(choose_full);
    r.internal_full_fallback.push_back(fallback);

    ++incremental_age;
    ++full_age;
    const double decayed_tail = 1.0 +
        (incremental_tail_ratio - 1.0) * (1.0 - kEmaAlpha);

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
      latest_full_us = execution_us;
      full_age = 0;
      previous_affected_fraction = 0.0;
      incremental_tail_ratio = decayed_tail;
      // A preflight/full batch cannot confirm whether the incremental tail was
      // transient, so a pending confirmation remains pending.
    } else {
      double normalized_tail = 1.0;
      if (t.predicted_incremental_us > 0.0) {
        const double raw_tail = execution_us /
            std::max(1.0, t.predicted_incremental_us);
        normalized_tail = std::max(1.0, raw_tail / t.affected_scale);
      }
      t.normalized_tail_ratio = normalized_tail;

      if (confirmation_batch) {
        // Confirmation consumes the pending probe. Persist only the decayed or
        // newly observed tail magnitude; do not immediately schedule another
        // confirmation for the same event.
        tail_confirmation_due = false;
        incremental_tail_ratio = std::max(decayed_tail, normalized_tail);
      } else if (normalized_tail >= kLearnedFullMargin) {
        incremental_tail_ratio = std::max(decayed_tail, normalized_tail);
        tail_confirmation_due = true;
      } else {
        incremental_tail_ratio = decayed_tail;
      }

      const double alpha = kEmaAlpha / std::max(1.0, t.affected_scale);
      ema_incremental_us = have_incremental
          ? (1.0 - alpha) * ema_incremental_us + alpha * execution_us
          : execution_us;
      have_incremental = true;
      deferred_after_preflight = false;
      incremental_age = 0;
      last_incremental_update_fraction = t.update_fraction;
      previous_affected_fraction = observed_affected_fraction;
    }

    r.traces.push_back(t);
    if (full_bfs(graph, root) != bfs.distances()) r.exact = false;
    first_batch = false;
  }
  return r;
}

void print_l3_bools(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_l3_traces(const std::vector<L3Trace>& traces) {
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
              << ",\"tail_adjusted_incremental_us\":" << t.tail_adjusted_incremental_us
              << ",\"incoming_tail_ratio\":" << t.incoming_tail_ratio
              << ",\"observed_affected_fraction\":" << t.observed_affected_fraction
              << ",\"affected_scale\":" << t.affected_scale
              << ",\"normalized_tail_ratio\":" << t.normalized_tail_ratio
              << ",\"incremental_age\":" << t.incremental_age
              << ",\"full_age\":" << t.full_age
              << ",\"tail_confirmation_due\":" << (t.tail_confirmation_due ? "true" : "false")
              << ",\"tail_confirmation_batch\":" << (t.tail_confirmation_batch ? "true" : "false")
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

  std::vector<L3Result> results;
  results.push_back(run_l3_baseline("always_incremental", edges, vertices, root,
                                    imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_l3_baseline("always_full", edges, vertices, root,
                                    imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_l3_baseline("simple_threshold", edges, vertices, root,
                                    imported_edges, batch_size, simple_update_fraction));
  results.push_back(run_adaptive_l3(edges, vertices, root, imported_edges,
                                    batch_size, simple_update_fraction));

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":8"
            << ",\"artifact_type\":\"velographx-publication-repair-recompute-policy\""
            << ",\"selector\":\"publication-preflight-bfs-v3-light3\""
            << ",\"parent_selector\":\"publication-preflight-bfs-v3-light2\""
            << ",\"selector_change\":\"affected-work-discounted robust incremental learning plus one-probe normalized residual tail guard; no forced stale-full refresh\""
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
    print_l3_bools(r.explicit_full);
    std::cout << ",\"internal_full_fallback\":";
    print_l3_bools(r.internal_full_fallback);
    std::cout << ",\"trace\":";
    print_l3_traces(r.traces);
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"oracle_full\":";
  print_l3_bools(oracle_full);
  std::cout << ",\"selector_feature_cost_included_in_adaptive_timing\":true"
            << ",\"verification_excluded_from_timing\":true"
            << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
