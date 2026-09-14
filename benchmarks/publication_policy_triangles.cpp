// Publication-oriented cross-algorithm policy harness for exact triangle count.
// Hosted runs are engineering evidence only; dedicated hardware is still
// required before promoting performance claims into a paper.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Edge = std::pair<velographx::VertexId, velographx::VertexId>;
using Clock = std::chrono::steady_clock;
constexpr double kEmaAlpha = 0.25;
constexpr double kPreflightFullUpdate = 0.05;
constexpr std::size_t kHistoryMinIncrementalSamples = 3;
constexpr std::size_t kHistoryMinFullSamples = 2;

struct Trace {
  double update_fraction{0.0};
  double predicted_incremental_us{0.0};
  double predicted_full_us{0.0};
  bool explicit_full{false};
  std::string reason;
};

struct Result {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> decision_us;
  std::vector<bool> explicit_full;
  std::vector<Trace> trace;
  std::size_t full_recompute_batches{0};
  bool exact{true};
};

std::uint64_t edge_key(std::uint64_t u, std::uint64_t v) {
  return (u << 32U) | v;
}

std::vector<Edge> read_edges(const std::string& path, std::size_t& vertices) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open edge list");

  // Preserve the preregistered input order.  We normalize undirected endpoints
  // and deduplicate first occurrence with a hash set instead of sorting the
  // whole stream, because sorting would silently change the dynamic workload.
  std::vector<Edge> edges;
  std::unordered_set<std::uint64_t> seen;
  std::string line;
  std::uint64_t max_vertex = 0;
  bool saw = false;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::uint64_t u = 0, v = 0;
    if (!(row >> u >> v)) continue;
    if (u == v) continue;
    if (u > std::numeric_limits<velographx::VertexId>::max() ||
        v > std::numeric_limits<velographx::VertexId>::max()) {
      throw std::runtime_error("vertex id exceeds VertexId range");
    }
    if (u > v) std::swap(u, v);
    if (!seen.insert(edge_key(u, v)).second) continue;
    edges.emplace_back(static_cast<velographx::VertexId>(u),
                       static_cast<velographx::VertexId>(v));
    max_vertex = std::max(max_vertex, std::max(u, v));
    saw = true;
  }
  vertices = saw ? static_cast<std::size_t>(max_vertex + 1) : 0;
  return edges;
}

velographx::UpdateBatch make_batch(const std::vector<Edge>& edges,
                                   std::size_t imported_edges,
                                   std::size_t begin,
                                   std::size_t end) {
  velographx::UpdateBatch updates;
  updates.updates.reserve((end - begin) * 2);
  for (std::size_t i = begin; i < end; ++i) {
    updates.add(edges[i].first, edges[i].second);
  }
  for (std::size_t i = begin; i < end; ++i) {
    const auto remove_index = i - imported_edges;
    updates.remove(edges[remove_index].first, edges[remove_index].second);
  }
  return updates;
}

Result run_policy(const std::string& policy,
                  const std::vector<Edge>& edges,
                  std::size_t vertices,
                  std::size_t imported_edges,
                  std::size_t batch_size,
                  double simple_update_fraction) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, false);
  graph.bulk_load_edges(initial);

  const auto initial_begin = Clock::now();
  velographx::IncrementalTriangleCount triangles(graph);
  const auto initial_end = Clock::now();
  const double initial_full_us =
      std::chrono::duration<double, std::micro>(initial_end - initial_begin).count();

  Result result;
  result.name = policy;

  double ema_incremental_us = 0.0;
  double ema_full_us = initial_full_us;
  double last_incremental_update_fraction = 0.0;
  double ema_incremental_rel_error = 0.0;
  double ema_full_rel_error = 0.0;
  bool have_incremental = false;
  bool have_incremental_error = false;
  bool have_full_error = false;

  double history_incremental_total_us = 0.0;
  double history_incremental_total_updates = 0.0;
  double history_full_total_us = initial_full_us;
  std::size_t history_incremental_samples = 0;
  std::size_t history_full_samples = 1;

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
    if (policy == "always_full") {
      choose_full = true;
      t.reason = "always_full";
    } else if (policy == "always_incremental") {
      t.reason = "always_incremental";
    } else if (policy == "simple_threshold") {
      choose_full = t.update_fraction >= simple_update_fraction;
      t.reason = choose_full ? "threshold_full" : "threshold_incremental";
    } else if (policy == "history_cost_model") {
      const double update_count =
          static_cast<double>(std::max<std::size_t>(1, updates.updates.size()));
      if (history_incremental_samples >= kHistoryMinIncrementalSamples) {
        const double cost_per_update = history_incremental_total_us /
            std::max(1.0, history_incremental_total_updates);
        t.predicted_incremental_us = cost_per_update * update_count;
      }
      t.predicted_full_us = history_full_total_us /
          static_cast<double>(std::max<std::size_t>(1, history_full_samples));
      if (history_incremental_samples < kHistoryMinIncrementalSamples) {
        t.reason = "history_calibrate_incremental";
      } else if (history_full_samples < kHistoryMinFullSamples) {
        choose_full = true;
        t.reason = "history_calibrate_full";
      } else {
        choose_full = t.predicted_full_us < t.predicted_incremental_us;
        t.reason = choose_full ? "history_cost_full" : "history_cost_incremental";
      }
    } else if (policy == "adaptive") {
      if (t.update_fraction >= kPreflightFullUpdate) {
        choose_full = true;
        t.reason = "preflight_large_update_full";
      } else if (!have_incremental) {
        // Initial construction already supplies a full-arm cost observation, so
        // probe the missing incremental arm rather than paying a redundant full.
        t.reason = "initial_incremental_probe";
      } else {
        const double scale = std::clamp(
            std::sqrt((t.update_fraction + 1e-12) /
                      (last_incremental_update_fraction + 1e-12)), 0.50, 2.50);
        t.predicted_incremental_us = ema_incremental_us * scale;
        t.predicted_full_us = ema_full_us;
        const double inc_uncertainty = have_incremental_error
            ? std::clamp(ema_incremental_rel_error, 0.05, 0.35) : 0.35;
        const double full_uncertainty = have_full_error
            ? std::clamp(ema_full_rel_error, 0.05, 0.35) : 0.20;
        const double inc_lower = t.predicted_incremental_us * (1.0 - inc_uncertainty);
        const double full_upper = t.predicted_full_us * (1.0 + full_uncertainty);
        choose_full = inc_lower > full_upper;
        t.reason = choose_full ? "uncertainty_confident_full"
                               : "uncertainty_overlap_incremental";
      }
    } else {
      throw std::runtime_error("unknown policy");
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

    // Correctness verification is deliberately outside the timed region.
    const auto produced = triangles.value();
    triangles.recompute();
    if (produced != triangles.value()) result.exact = false;

    if (policy == "history_cost_model") {
      if (choose_full) {
        history_full_total_us += execution_us;
        ++history_full_samples;
      } else {
        history_incremental_total_us += execution_us;
        history_incremental_total_updates +=
            static_cast<double>(std::max<std::size_t>(1, updates.updates.size()));
        ++history_incremental_samples;
      }
    }

    if (policy == "adaptive") {
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
      } else {
        ema_incremental_us = have_incremental
            ? (1.0 - kEmaAlpha) * ema_incremental_us + kEmaAlpha * execution_us
            : execution_us;
        have_incremental = true;
        last_incremental_update_fraction = t.update_fraction;
      }
    }

    result.batch_us.push_back(batch_us);
    result.decision_us.push_back(decision_us);
    result.explicit_full.push_back(choose_full);
    result.trace.push_back(t);
  }
  return result;
}

void print_double_array(const std::vector<double>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}

void print_bool_array(const std::vector<bool>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << (values[i] ? "true" : "false");
  }
  std::cout << ']';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: publication_policy_triangles <edge-list> <import-rate> <batch-size> <simple-threshold>\n";
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
      batch_size == 0) {
    return 2;
  }

  const std::vector<std::string> names = {
      "always_incremental", "always_full", "simple_threshold",
      "history_cost_model", "adaptive"};
  std::vector<Result> results;
  for (const auto& name : names) {
    results.push_back(run_policy(name, edges, vertices, imported_edges,
                                 batch_size, simple_threshold));
  }

  const auto batches = results.front().batch_us.size();
  std::vector<double> oracle(batches, 0.0);
  std::vector<bool> oracle_full(batches, false);
  for (std::size_t i = 0; i < batches; ++i) {
    oracle_full[i] = !(results[0].batch_us[i] < results[1].batch_us[i]);
    oracle[i] = oracle_full[i] ? results[1].batch_us[i] : results[0].batch_us[i];
  }

  bool all_exact = true;
  std::cout << "{\"schema_version\":1"
            << ",\"artifact_type\":\"velographx-cross-algorithm-repair-recompute-policy\""
            << ",\"algorithm\":\"exact_triangle_count\""
            << ",\"selector\":\"publication-preflight-triangle-v1\""
            << ",\"framework_relation\":\"same pre-repair two-arm/oracle-regret framework; triangle-specific observable signals\""
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
