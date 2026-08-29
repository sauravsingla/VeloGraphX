#include <cassert>
#include <cmath>

#include "velographx/incremental/pagerank.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {

void assert_close(const std::vector<double>& a, const std::vector<double>& b,
                  double tol = 1e-8) {
  assert(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    assert(std::abs(a[i] - b[i]) < tol);
  }
}

double sum(const std::vector<double>& values) {
  double total = 0.0;
  for (auto value : values) total += value;
  return total;
}

}  // namespace

int main() {
  using namespace velographx;

  // Two disconnected directed regions. Updating the first should use explicit
  // incoming adjacency and remain localized because every vertex has nonzero
  // outdegree and no global dangling-mass term changes.
  DynamicGraph g(8, true);
  UpdateBatch initial;
  initial.add(0, 1);
  initial.add(1, 2);
  initial.add(2, 3);
  initial.add(3, 0);
  initial.add(4, 5);
  initial.add(5, 6);
  initial.add(6, 7);
  initial.add(7, 4);
  g.apply(initial);

  IncrementalPageRank incremental(g);
  assert(incremental.last_full_recompute_converged());
  assert(incremental.last_residual_linf() <= 1e-12);

  UpdateBatch update;
  update.add(0, 2);
  incremental.apply(update, 128, 1e-13, 0.95);
  assert(incremental.last_repair_iterations() > 1);
  assert(incremental.last_repaired_vertices() >= 3);
  assert(incremental.last_repaired_vertices() < g.vertex_count());

  // Quantitative reviewer-facing validation against an independently converged
  // full PageRank solve. Both the reference residual and vector error are
  // reported explicitly rather than inferred from iteration count.
  const auto validation = incremental.validate_against_full(
      1000, 1e-13, 1e-7, 1e-8);
  assert(validation.reference_converged);
  assert(validation.reference_residual_linf <= 1e-13);
  assert(validation.l1_error <= 1e-7);
  assert(validation.linf_error <= 1e-8);
  assert(validation.within_tolerance);
  assert(!validation.fallback_applied);

  // Force the affected-region safety fallback and compare against a separately
  // constructed graph solved to the same numerical tolerance.
  DynamicGraph full_graph(8, true);
  full_graph.apply(initial);
  full_graph.apply(update);
  IncrementalPageRank full(full_graph);
  full.recompute(1000, 1e-13);
  assert(full.last_full_recompute_converged());

  DynamicGraph fallback_graph(8, true);
  fallback_graph.apply(initial);
  IncrementalPageRank fallback(fallback_graph);
  fallback.apply(update, 64, 1e-12, 0.20);
  assert(fallback.last_repaired_vertices() == fallback_graph.vertex_count());
  assert(fallback.last_full_recompute_converged());
  assert_close(fallback.values(), full.values());

  // Dangling mass is a global dependency in PageRank. Turning a dangling node
  // into a non-dangling node must therefore trigger a full solve instead of a
  // falsely localized repair. The converged distribution must still sum to 1.
  DynamicGraph dangling_graph(4, true);
  UpdateBatch dangling_initial;
  dangling_initial.add(0, 1);
  dangling_initial.add(1, 2);
  dangling_graph.apply(dangling_initial);  // vertices 2 and 3 are dangling
  IncrementalPageRank dangling(dangling_graph);
  assert(dangling.last_full_recompute_converged());
  assert(std::abs(sum(dangling.values()) - 1.0) < 1e-10);

  UpdateBatch dangling_change;
  dangling_change.add(2, 0);  // vertex 2 changes dangling status
  dangling.apply(dangling_change, 128, 1e-13, 0.95);
  assert(dangling.last_repaired_vertices() == dangling_graph.vertex_count());
  assert(dangling.last_full_recompute_converged());
  assert(dangling.last_residual_linf() <= 1e-12);
  assert(std::abs(sum(dangling.values()) - 1.0) < 1e-10);

  // Validation mode guarantees a result satisfying the requested error
  // contract. If localized repair misses it, apply_validated installs the full
  // reference and reports fallback_applied.
  DynamicGraph validated_graph(8, true);
  validated_graph.apply(initial);
  IncrementalPageRank validated(validated_graph);
  const auto validated_metrics = validated.apply_validated(
      update, 1, 1e-3, 0.99, 1000, 1e-13, 1e-12, 1e-13);
  assert(validated_metrics.reference_converged);
  assert(validated_metrics.fallback_applied || validated_metrics.within_tolerance);
  const auto post_validation = validated.validate_against_full(
      1000, 1e-13, 1e-12, 1e-13);
  assert(post_validation.within_tolerance);

  return 0;
}
