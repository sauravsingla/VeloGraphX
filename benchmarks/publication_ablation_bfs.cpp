// Clean same-stream A0-A7 causal ablation for the reviewer-facing paper path.
//
// Historical selector-development runs remain motivation only. Hosted CI numbers
// are engineering evidence; the identical harness must be rerun on controlled
// publication hardware before performance claims are promoted.

#include <numeric>
#include <unordered_set>

#include "velographx/csr_graph.hpp"

#define main velographx_frozen_adaptive_policy_main_for_ablation
#include "adaptive_policy_bfs.cpp"
#undef main

namespace {

struct AblationResult {
  std::string stage;
  std::string mechanism;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_fallback;
  std::size_t full_recompute_batches{0};
  std::size_t internal_fallback_batches{0};
  double fallback_execution_us{0.0};
  bool exact{true};
};

std::uint64_t ablation_edge_key(velographx::VertexId u, velographx::VertexId v) {
  return (static_cast<std::uint64_t>(u) << 32U) | static_cast<std::uint64_t>(v);
}

void print_ablation_bool_array(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

void print_json_string(const std::string& value) {
  std::cout << '"';
  for (const char c : value) {
    if (c == '"' || c == '\\') std::cout << '\\';
    std::cout << c;
  }
  std::cout << '"';
}

std::vector<std::uint32_t> csr_bfs(const velographx::CsrGraph& graph,
                                   velographx::VertexId source,
                                   std::size_t declared_vertices) {
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> dist(declared_vertices, unreachable);
  if (source >= graph.vertex_count()) return dist;
  std::queue<velographx::VertexId> q;
  dist[source] = 0;
  q.push(source);
  while (!q.empty()) {
    const auto u = q.front();
    q.pop();
    graph.for_each_neighbor(u, [&](velographx::VertexId v) {
      if (v < dist.size() && dist[v] == unreachable) {
        dist[v] = dist[u] + 1;
        q.push(v);
      }
    });
  }
  return dist;
}

AblationResult run_a0(const std::vector<Edge>& edges,
                      std::size_t vertices,
                      velographx::VertexId root,
                      std::size_t imported_edges,
                      std::size_t batch_size) {
  AblationResult result;
  result.stage = "A0";
  result.mechanism = "canonical CSR rebuild + full BFS";

  std::unordered_set<std::uint64_t> active;
  active.reserve(imported_edges * 2 + 1);
  for (std::size_t i = 0; i < imported_edges; ++i) {
    active.insert(ablation_edge_key(edges[i].first, edges[i].second));
  }

  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph reference_graph(vertices, true);
  reference_graph.bulk_load_edges(initial);

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    for (const auto& op : updates.updates) {
      const auto key = ablation_edge_key(op.src, op.dst);
      if (op.add) active.insert(key); else active.erase(key);
    }
    std::vector<Edge> snapshot;
    snapshot.reserve(active.size());
    for (const auto key : active) {
      snapshot.emplace_back(static_cast<velographx::VertexId>(key >> 32U),
                            static_cast<velographx::VertexId>(key & 0xFFFFFFFFULL));
    }
    velographx::CsrGraph csr(std::move(snapshot), true);
    const auto produced = csr_bfs(csr, root, vertices);
    const auto total_end = Clock::now();

    // Reference mutation and correctness verification stay outside timing.
    reference_graph.apply(updates);
    if (produced != full_bfs(reference_graph, root)) result.exact = false;

    result.batch_us.push_back(
        std::chrono::duration<double, std::micro>(total_end - total_begin).count());
    result.decision_us.push_back(0.0);
    result.explicit_full.push_back(true);
    result.internal_fallback.push_back(false);
    ++result.full_recompute_batches;
  }
  return result;
}

AblationResult run_dynamic_stage(int level,
                                 const std::vector<Edge>& edges,
                                 std::size_t vertices,
                                 velographx::VertexId root,
                                 std::size_t imported_edges,
                                 std::size_t batch_size) {
  static const std::vector<std::string> mechanisms = {
      "canonical CSR rebuild + full BFS",
      "compact mutable storage + full BFS",
      "localized exact repair",
      "localized repair + affected-work fallback",
      "+ graph-scale/root-state preflight",
      "+ online cost prediction",
      "+ uncertainty-aware choice",
      "+ selector-owned recomputation"};

  AblationResult result;
  result.stage = "A" + std::to_string(level);
  result.mechanism = mechanisms.at(static_cast<std::size_t>(level));

  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);

  // A2 isolates repair without affected-work fallback. A7 also disables the
  // hidden internal fallback so a full recomputation can only be selector-owned.
  const double repair_budget = (level == 2 || level == 7) ? 2.0 : kAffectedBudget;
  const auto initial_begin = Clock::now();
  velographx::IncrementalBFS bfs(graph, root, repair_budget);
  const auto initial_end = Clock::now();
  const double initial_full_us =
      std::chrono::duration<double, std::micro>(initial_end - initial_begin).count();

  const double reachable_fraction =
      static_cast<double>(reachable_vertices(bfs.distances())) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  const bool large_scale = vertices >= 200000;

  double previous_affected_fraction = 0.0;
  double ema_incremental_us = 0.0;
  double ema_full_us = initial_full_us;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  bool have_incremental = false;
  bool have_incremental_error = false;
  bool have_full_error = false;
  std::size_t incremental_age = kFreshAge + 1;
  std::size_t full_age = 0;
  bool first_batch = true;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    const auto total_begin = Clock::now();

    const double update_fraction = static_cast<double>(updates.updates.size()) /
        static_cast<double>(std::max<std::size_t>(1, graph.edge_count_directed()));
    const double shallow_fraction = first_batch
        ? shallow_parent_deletion_fraction(updates, bfs.distances(), graph.directed())
        : 0.0;

    bool choose_full = level == 1;
    double predicted_incremental = 0.0;
    double predicted_full = ema_full_us;
    const auto decision_begin = Clock::now();

    if (level >= 4) {
      // A4 introduces only observable structural/preflight guards.
      if (update_fraction >= kPreflightFullUpdate ||
          reachable_fraction <= kVerySparseReach ||
          (reachable_fraction < kSparseReach &&
           update_fraction >= kSparseReachFullUpdate) ||
          (large_scale && first_batch && shallow_fraction > 0.0)) {
        choose_full = true;
      }
    }

    if (!choose_full && level >= 5 && have_incremental) {
      // A5 introduces the online execution-cost prediction.
      const double scale = std::clamp(
          std::sqrt((update_fraction + 1e-12) /
                    (last_incremental_update_fraction + 1e-12)), 0.50, 2.50);
      predicted_incremental = ema_incremental_us * scale *
          (1.0 + (large_scale ? 3.0 : 2.0) * previous_affected_fraction);
      predicted_full = ema_full_us;

      if (level == 5) {
        const bool fresh = incremental_age <= kFreshAge && full_age <= kFreshAge;
        choose_full = fresh && predicted_incremental > predicted_full * kLearnedFullMargin;
      } else {
        // A6 adds prediction-error uncertainty bands. A7 reuses the same
        // decision but owns a full recomputation before repair begins.
        const double inc_uncertainty = have_incremental_error
            ? std::clamp(ema_incremental_rel_error, 0.05, 0.35) : 0.35;
        const double full_uncertainty = have_full_error
            ? std::clamp(ema_full_rel_error, 0.05, 0.35) : 0.20;
        choose_full = predicted_incremental * (1.0 - inc_uncertainty) >
                      predicted_full * (1.0 + full_uncertainty);
      }
    }

    // The initial full BFS already supplies a full-arm measurement. Do not pay
    // a second full warmup merely to calibrate the selector.
    if (level == 7 && !have_incremental && !choose_full && update_fraction < kPreflightFullUpdate) {
      choose_full = false;
    }

    const auto decision_end = Clock::now();
    const double decision_us =
        std::chrono::duration<double, std::micro>(decision_end - decision_begin).count();

    const auto execution_begin = Clock::now();
    bool internal_fallback = false;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++result.full_recompute_batches;
    } else {
      bfs.apply(updates);
      internal_fallback = bfs.last_used_full_recompute();
      if (internal_fallback) {
        ++result.full_recompute_batches;
        ++result.internal_fallback_batches;
      }
    }
    const auto execution_end = Clock::now();
    const double execution_us =
        std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();
    const double batch_us =
        std::chrono::duration<double, std::micro>(execution_end - total_begin).count();
    if (internal_fallback) result.fallback_execution_us += execution_us;

    if (level >= 5) {
      ++incremental_age;
      ++full_age;
      const bool observed_full = choose_full || internal_fallback;
      if (predicted_incremental > 0.0 && predicted_full > 0.0) {
        if (observed_full) {
          const double rel = std::abs(execution_us - predicted_full) /
              std::max(1.0, predicted_full);
          ema_full_rel_error = have_full_error
              ? (1.0 - kEmaAlpha) * ema_full_rel_error + kEmaAlpha * rel : rel;
          have_full_error = true;
        } else {
          const double rel = std::abs(execution_us - predicted_incremental) /
              std::max(1.0, predicted_incremental);
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
        last_incremental_update_fraction = update_fraction;
        previous_affected_fraction =
            static_cast<double>(bfs.last_affected_vertices()) /
            static_cast<double>(std::max<std::size_t>(1, vertices));
      }
    }

    if (full_bfs(graph, root) != bfs.distances()) result.exact = false;
    result.batch_us.push_back(batch_us);
    result.decision_us.push_back(decision_us);
    result.explicit_full.push_back(choose_full);
    result.internal_fallback.push_back(internal_fallback);
    first_batch = false;
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: publication_ablation_bfs <edge-list> <root> <import-rate> <batch-size>\n";
    return 2;
  }
  const std::string path = argv[1];
  const auto root64 = std::stoull(argv[2]);
  const double imported_rate = std::stod(argv[3]);
  const auto batch_size = static_cast<std::size_t>(std::stoull(argv[4]));
  if (root64 > std::numeric_limits<velographx::VertexId>::max()) return 2;
  const auto root = static_cast<velographx::VertexId>(root64);

  std::size_t vertices = 0;
  const auto edges = read_edges(path, vertices);
  const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
  if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size() || batch_size == 0) return 2;

  std::vector<AblationResult> results;
  results.push_back(run_a0(edges, vertices, root, imported_edges, batch_size));
  for (int level = 1; level <= 7; ++level) {
    results.push_back(run_dynamic_stage(level, edges, vertices, root, imported_edges, batch_size));
  }

  // A1 and A2 isolate the two executable arms while sharing the mutable store.
  const auto& full = results[1];
  const auto& incremental = results[2];
  const auto batches = full.batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle[i] = std::min(full.batch_us[i], incremental.batch_us[i]);
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":1"
            << ",\"artifact_type\":\"velographx-clean-a0-a7-ablation\""
            << ",\"oracle_definition\":\"min(A1 mutable-storage full, A2 localized repair)\""
            << ",\"root\":" << root64
            << ",\"vertices\":" << vertices
            << ",\"batch_size\":" << batch_size
            << ",\"batches\":" << batches
            << ",\"research_claim\":false"
            << ",\"stages\":[";

  for (std::size_t s = 0; s < results.size(); ++s) {
    const auto& r = results[s];
    if (s) std::cout << ',';
    all_exact = all_exact && r.exact;
    double regret_sum = 0.0;
    double max_regret = 0.0;
    for (std::size_t i = 0; i < r.batch_us.size(); ++i) {
      const double regret = oracle[i] > 0.0
          ? std::max(0.0, (r.batch_us[i] - oracle[i]) / oracle[i]) : 0.0;
      regret_sum += regret;
      max_regret = std::max(max_regret, regret);
    }
    const double total = std::accumulate(r.batch_us.begin(), r.batch_us.end(), 0.0);
    const double decisions = std::accumulate(r.decision_us.begin(), r.decision_us.end(), 0.0);

    std::cout << "{\"stage\":";
    print_json_string(r.stage);
    std::cout << ",\"mechanism\":";
    print_json_string(r.mechanism);
    std::cout << ",\"exact\":" << (r.exact ? "true" : "false")
              << ",\"mean_batch_us\":" << total / std::max<std::size_t>(1, batches)
              << ",\"mean_decision_us\":" << decisions / std::max<std::size_t>(1, batches)
              << ",\"mean_oracle_regret\":" << regret_sum / std::max<std::size_t>(1, batches)
              << ",\"max_oracle_regret\":" << max_regret
              << ",\"full_recompute_batches\":" << r.full_recompute_batches
              << ",\"internal_fallback_batches\":" << r.internal_fallback_batches
              << ",\"fallback_execution_us\":" << r.fallback_execution_us
              << ",\"batch_us\":";
    print_array(r.batch_us);
    std::cout << ",\"explicit_full\":";
    print_ablation_bool_array(r.explicit_full);
    std::cout << ",\"internal_fallback\":";
    print_ablation_bool_array(r.internal_fallback);
    std::cout << '}';
  }

  std::cout << "],\"oracle_batch_us\":";
  print_array(oracle);
  std::cout << ",\"verification_excluded_from_timing\":true"
            << ",\"historical_runs_used_as_causal_ablation\":false"
            << ",\"all_stages_exact\":" << (all_exact ? "true" : "false")
            << "}\n";
  return all_exact ? 0 : 1;
}
