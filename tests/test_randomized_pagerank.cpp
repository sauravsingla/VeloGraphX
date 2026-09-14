#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <vector>

#include "velographx/incremental/pagerank.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

using velographx::DynamicGraph;
using velographx::IncrementalPageRank;
using velographx::UpdateBatch;
using velographx::VertexId;

std::size_t env_size(const char* name, std::size_t fallback) {
  const char* value = std::getenv(name);
  if (!value || *value == '\0') return fallback;
  char* end = nullptr;
  const auto parsed = std::strtoull(value, &end, 10);
  if (end == value || *end != '\0' || parsed == 0) return fallback;
  return static_cast<std::size_t>(parsed);
}

std::vector<double> reference_pagerank(const DynamicGraph& graph,
                                       double damping = 0.85,
                                       std::size_t max_iterations = 2000,
                                       double tol = 1e-14) {
  const auto n = graph.vertex_count();
  if (n == 0) return {};

  std::vector<double> rank(n, 1.0 / static_cast<double>(n));
  std::vector<std::size_t> out_degree(n, 0);
  for (VertexId u = 0; u < n; ++u) out_degree[u] = graph.neighbors(u).size();

  const double teleport = (1.0 - damping) / static_cast<double>(n);
  for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
    double dangling_mass = 0.0;
    for (VertexId u = 0; u < n; ++u) {
      if (out_degree[u] == 0) dangling_mass += rank[u];
    }

    std::vector<double> next(
        n, teleport + damping * dangling_mass / static_cast<double>(n));
    for (VertexId u = 0; u < n; ++u) {
      if (out_degree[u] == 0) continue;
      const double contribution =
          damping * rank[u] / static_cast<double>(out_degree[u]);
      for (const auto v : graph.neighbors(u)) next[v] += contribution;
    }

    double residual_linf = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      residual_linf = std::max(residual_linf, std::abs(next[i] - rank[i]));
    }
    rank.swap(next);
    if (residual_linf <= tol) break;
  }
  return rank;
}

void assert_distribution(const std::vector<double>& rank) {
  double total = 0.0;
  for (const auto value : rank) {
    assert(std::isfinite(value));
    assert(value >= -1e-12);
    total += value;
  }
  assert(std::abs(total - 1.0) <= 2e-7);
}

void assert_matches_reference(const IncrementalPageRank& incremental,
                              const DynamicGraph& graph,
                              double l1_tolerance = 2e-7,
                              double linf_tolerance = 2e-8) {
  const auto reference = reference_pagerank(graph);
  const auto& actual = incremental.values();
  assert(actual.size() == reference.size());

  double l1 = 0.0;
  double linf = 0.0;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    const double error = std::abs(actual[i] - reference[i]);
    l1 += error;
    linf = std::max(linf, error);
  }
  assert(l1 <= l1_tolerance);
  assert(linf <= linf_tolerance);
  assert_distribution(actual);
}

void add_component_backbone(UpdateBatch& batch, VertexId begin,
                            VertexId component_size) {
  for (VertexId offset = 0; offset < component_size; ++offset) {
    const auto u = static_cast<VertexId>(begin + offset);
    const auto v = static_cast<VertexId>(
        begin + ((offset + 1) % component_size));
    batch.add(u, v);
  }
}

void run_seed(std::uint32_t seed, std::size_t operations) {
  constexpr VertexId component_size = 6;
  constexpr VertexId component_count = 6;
  constexpr VertexId vertices = component_size * component_count;

  DynamicGraph graph(vertices, true);
  UpdateBatch initial;
  for (VertexId component = 0; component < component_count; ++component) {
    add_component_backbone(initial,
                           static_cast<VertexId>(component * component_size),
                           component_size);
  }

  std::mt19937 rng(seed);
  std::uniform_int_distribution<std::uint32_t> component_dist(
      0, component_count - 1);
  std::uniform_int_distribution<std::uint32_t> offset_dist(
      0, component_size - 1);
  std::bernoulli_distribution add_dist(0.58);
  std::uniform_int_distribution<std::uint32_t> batch_size_dist(1, 4);

  // Add deterministic-random non-backbone edges while keeping every vertex
  // non-dangling. This makes localized repair a valid optimization because
  // updates stay inside disconnected directed components.
  for (std::size_t i = 0; i < 72; ++i) {
    const auto component = static_cast<VertexId>(component_dist(rng));
    const auto begin = static_cast<VertexId>(component * component_size);
    const auto u = static_cast<VertexId>(begin + offset_dist(rng));
    auto v = static_cast<VertexId>(begin + offset_dist(rng));
    if (v == u) v = static_cast<VertexId>(begin + ((v - begin + 2) % component_size));
    const auto backbone = static_cast<VertexId>(
        begin + ((u - begin + 1) % component_size));
    if (v == backbone) v = static_cast<VertexId>(
        begin + ((v - begin + 1) % component_size));
    if (v != u) initial.add(u, v);
  }
  graph.apply(initial);

  IncrementalPageRank incremental(graph);
  assert_matches_reference(incremental, graph);

  std::size_t localized_updates = 0;
  for (std::size_t step = 0; step < operations; ++step) {
    const auto component = static_cast<VertexId>(component_dist(rng));
    const auto begin = static_cast<VertexId>(component * component_size);
    UpdateBatch batch;
    const auto batch_size = batch_size_dist(rng);

    VertexId repeated_u = begin;
    VertexId repeated_v = static_cast<VertexId>(begin + 2);
    for (std::uint32_t item = 0; item < batch_size; ++item) {
      const auto u = static_cast<VertexId>(begin + offset_dist(rng));
      auto v = static_cast<VertexId>(begin + offset_dist(rng));
      if (v == u) v = static_cast<VertexId>(
          begin + ((v - begin + 2) % component_size));

      const bool add = add_dist(rng);
      const auto backbone = static_cast<VertexId>(
          begin + ((u - begin + 1) % component_size));
      if (!add && v == backbone) {
        v = static_cast<VertexId>(
            begin + ((v - begin + 1) % component_size));
      }
      if (v == u) continue;

      if (add) batch.add(u, v, step + 1);
      else batch.remove(u, v, step + 1);
      repeated_u = u;
      repeated_v = v;
    }

    // Periodically exercise repeated updates to one edge in the same batch.
    // The final graph state, not an intermediate operation, must determine the
    // maintained PageRank result.
    if ((step % 19) == 0 && repeated_u != repeated_v) {
      batch.add(repeated_u, repeated_v, step + 1);
      batch.remove(repeated_u, repeated_v, step + 1);
      batch.add(repeated_u, repeated_v, step + 1);
    }

    const auto validation = incremental.apply_validated(
        batch, 128, 1e-12, 0.95, 1000, 1e-13, 1e-7, 1e-8);
    assert(validation.reference_converged);
    assert(validation.within_tolerance || validation.fallback_applied);
    assert_matches_reference(incremental, graph);

    if (!validation.fallback_applied &&
        incremental.last_repaired_vertices() < graph.vertex_count()) {
      ++localized_updates;
    }
  }
  assert(localized_updates != 0);

  // Randomized forced dangling transitions exercise the graph-wide fallback.
  // Restoring the cycle edge on the next update also requires a global solve
  // because dangling mass existed in the pre-update rank state.
  const auto dangling_rounds = std::max<std::size_t>(4, operations / 40);
  for (std::size_t round = 0; round < dangling_rounds; ++round) {
    const auto component = static_cast<VertexId>(component_dist(rng));
    const auto begin = static_cast<VertexId>(component * component_size);
    const auto u = static_cast<VertexId>(begin + offset_dist(rng));

    UpdateBatch make_dangling;
    const auto outgoing = graph.neighbors(u);
    for (const auto v : outgoing) make_dangling.remove(u, v, round + 1);
    incremental.apply(make_dangling, 128, 1e-12, 0.95);
    assert(incremental.last_repaired_vertices() == graph.vertex_count());
    assert(incremental.last_full_recompute_converged());
    assert_matches_reference(incremental, graph);

    UpdateBatch restore;
    const auto backbone = static_cast<VertexId>(
        begin + ((u - begin + 1) % component_size));
    restore.add(u, backbone, round + 1);
    incremental.apply(restore, 128, 1e-12, 0.95);
    assert(incremental.last_repaired_vertices() == graph.vertex_count());
    assert(incremental.last_full_recompute_converged());
    assert_matches_reference(incremental, graph);
  }
}

}  // namespace

int main() {
  const auto operations =
      env_size("VELOGRAPHX_PAGERANK_RANDOMIZED_OPS", 300);
  const auto seeds = env_size("VELOGRAPHX_PAGERANK_RANDOMIZED_SEEDS", 4);
  for (std::size_t seed = 1; seed <= seeds; ++seed) {
    run_seed(static_cast<std::uint32_t>(seed * 104729U), operations);
  }
  return 0;
}
