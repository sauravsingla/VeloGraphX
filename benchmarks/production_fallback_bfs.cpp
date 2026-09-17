// Deployment-style validation of the production IncrementalBFS fallback.
//
// This harness deliberately does not reimplement the publication selector.
// A selector decision schedule is produced by publication_policy_bfs.cpp with
// fallback disabled, then replayed here against the production 0.35 fallback.
// This isolates the practical question: how much repair-then-full work does a
// pre-repair full choice avoid while preserving the same exact semantics?
#define main velographx_frozen_adaptive_policy_main
#include "adaptive_policy_bfs.cpp"
#undef main

#include <fstream>
#include <numeric>

namespace {

struct ScheduleEntry {
  bool explicit_full{false};
  double selector_us{0.0};
};

struct DeploymentResult {
  std::string name;
  std::vector<double> batch_us;
  std::vector<double> execution_us;
  std::vector<double> selector_us;
  std::vector<bool> explicit_full;
  std::vector<bool> internal_fallback;
  std::vector<std::size_t> affected_vertices;
  std::size_t explicit_full_batches{0};
  std::size_t internal_fallback_batches{0};
  bool exact{true};
};

std::vector<ScheduleEntry> read_schedule(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open selector schedule: " + path);
  std::vector<ScheduleEntry> schedule;
  int full = 0;
  double selector_us = 0.0;
  while (in >> full >> selector_us) {
    if (full != 0 && full != 1) {
      throw std::runtime_error("selector schedule full flag must be 0 or 1");
    }
    if (selector_us < 0.0) {
      throw std::runtime_error("selector schedule cost must be non-negative");
    }
    schedule.push_back({full == 1, selector_us});
  }
  if (schedule.empty()) throw std::runtime_error("selector schedule is empty");
  return schedule;
}

DeploymentResult run_deployment_policy(
    const std::string& policy,
    const std::vector<Edge>& edges,
    std::size_t vertices,
    velographx::VertexId root,
    std::size_t imported_edges,
    std::size_t batch_size,
    double fallback_fraction,
    const std::vector<ScheduleEntry>& schedule) {
  std::vector<Edge> initial(edges.begin(), edges.begin() + imported_edges);
  velographx::DynamicGraph graph(vertices, true);
  graph.bulk_load_edges(initial);
  velographx::IncrementalBFS bfs(graph, root, fallback_fraction);

  DeploymentResult result;
  result.name = policy;
  std::size_t batch_index = 0;

  for (std::size_t begin = imported_edges; begin < edges.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, edges.size());
    auto updates = make_batch(edges, imported_edges, begin, end);
    if (batch_index >= schedule.size()) {
      throw std::runtime_error("selector schedule shorter than workload");
    }

    const bool choose_full =
        policy == "always_full" ||
        (policy == "selector_plus_fallback" && schedule[batch_index].explicit_full);
    const double selector_us =
        policy == "selector_plus_fallback" ? schedule[batch_index].selector_us : 0.0;

    const auto execution_begin = Clock::now();
    bool fallback = false;
    std::size_t affected = 0;
    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++result.explicit_full_batches;
    } else {
      bfs.apply(updates);
      fallback = bfs.last_used_full_recompute();
      affected = bfs.last_affected_vertices();
      if (fallback) ++result.internal_fallback_batches;
    }
    const auto execution_end = Clock::now();
    const double execution_us =
        std::chrono::duration<double, std::micro>(execution_end - execution_begin).count();

    result.execution_us.push_back(execution_us);
    result.selector_us.push_back(selector_us);
    result.batch_us.push_back(execution_us + selector_us);
    result.explicit_full.push_back(choose_full);
    result.internal_fallback.push_back(fallback);
    result.affected_vertices.push_back(affected);

    const auto reference = full_bfs(graph, root);
    if (reference != bfs.distances()) result.exact = false;
    ++batch_index;
  }

  if (batch_index != schedule.size()) {
    throw std::runtime_error("selector schedule longer than workload");
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

void print_size_vector(const std::vector<std::size_t>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 7) {
    std::cerr << "usage: " << argv[0]
              << " EDGE_LIST ROOT IMPORT_RATE BATCH_SIZE FALLBACK_FRACTION SCHEDULE\n";
    return 2;
  }

  try {
    const std::string path = argv[1];
    const auto root64 = std::stoull(argv[2]);
    const double imported_rate = std::stod(argv[3]);
    const auto batch_size = static_cast<std::size_t>(std::stoull(argv[4]));
    const double fallback_fraction = std::stod(argv[5]);
    const std::string schedule_path = argv[6];
    if (root64 > std::numeric_limits<velographx::VertexId>::max() ||
        imported_rate <= 0.0 || imported_rate >= 1.0 ||
        batch_size == 0 || fallback_fraction <= 0.0 || fallback_fraction > 1.0) {
      return 2;
    }
    const auto root = static_cast<velographx::VertexId>(root64);

    std::size_t vertices = 0;
    const auto edges = read_edges(path, vertices);
    const auto imported_edges = static_cast<std::size_t>(edges.size() * imported_rate);
    if (edges.empty() || imported_edges == 0 || imported_edges >= edges.size()) return 2;
    const auto schedule = read_schedule(schedule_path);

    const std::vector<std::string> names = {
        "fallback_only", "selector_plus_fallback", "always_full"};
    std::vector<DeploymentResult> results;
    for (const auto& name : names) {
      results.push_back(run_deployment_policy(
          name, edges, vertices, root, imported_edges, batch_size,
          fallback_fraction, schedule));
    }

    const auto batches = results.front().batch_us.size();
    bool all_exact = true;
    std::cout << "{\"schema_version\":1"
              << ",\"artifact_type\":\"velographx-production-fallback-replay\""
              << ",\"selector_source\":\"publication-preflight-v1 decision replay\""
              << ",\"fallback_fraction\":" << fallback_fraction
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
                << ",\"mean_batch_us\":"
                << total_us / std::max<std::size_t>(1, batches)
                << ",\"explicit_full_batches\":" << result.explicit_full_batches
                << ",\"internal_fallback_batches\":" << result.internal_fallback_batches
                << ",\"batch_us\":";
      print_array(result.batch_us);
      std::cout << ",\"execution_us\":";
      print_array(result.execution_us);
      std::cout << ",\"selector_us\":";
      print_array(result.selector_us);
      std::cout << ",\"explicit_full\":";
      print_bool_vector(result.explicit_full);
      std::cout << ",\"internal_full_fallback\":";
      print_bool_vector(result.internal_fallback);
      std::cout << ",\"affected_vertices\":";
      print_size_vector(result.affected_vertices);
      std::cout << '}';
    }

    std::cout << "],\"verification_excluded_from_timing\":true"
              << ",\"selector_cost_included_for_selector_plus_fallback\":true"
              << ",\"all_policies_exact\":" << (all_exact ? "true" : "false")
              << "}\n";
    return all_exact ? 0 : 1;
  } catch (const std::exception& exc) {
    std::cerr << "error: " << exc.what() << '\n';
    return 2;
  }
}
