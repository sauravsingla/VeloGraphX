#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "velographx/graph_access.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

struct PageRankValidation {
  double l1_error{0.0};
  double linf_error{0.0};
  double local_residual_l1{0.0};
  double local_residual_linf{0.0};
  double reference_residual_l1{0.0};
  double reference_residual_linf{0.0};
  std::size_t reference_iterations{0};
  bool reference_converged{false};
  bool within_tolerance{false};
  bool fallback_applied{false};
};

template <class Graph>
class BasicIncrementalPageRank {
 public:
  explicit BasicIncrementalPageRank(Graph& g, double damping = 0.85)
      : g_(g), damping_(damping) {
    recompute();
  }

  [[nodiscard]] const std::vector<double>& values() const noexcept { return rank_; }
  [[nodiscard]] std::size_t last_repaired_vertices() const noexcept { return last_repaired_vertices_; }
  [[nodiscard]] std::size_t last_repair_iterations() const noexcept { return last_repair_iterations_; }
  [[nodiscard]] double last_residual_l1() const noexcept { return last_residual_l1_; }
  [[nodiscard]] double last_residual_linf() const noexcept { return last_residual_linf_; }
  [[nodiscard]] bool last_full_recompute_converged() const noexcept { return last_full_recompute_converged_; }

  void apply(const UpdateBatch& batch, std::size_t local_iterations = 24,
             double tol = 1e-9, double full_fallback_fraction = 0.60) {
    if (batch.empty()) {
      last_repaired_vertices_ = 0;
      last_repair_iterations_ = 0;
      last_residual_l1_ = 0.0;
      last_residual_linf_ = 0.0;
      return;
    }

    std::vector<std::pair<VertexId, bool>> dangling_before;
    dangling_before.reserve(batch.updates.size() * (is_directed(g_) ? 1 : 2));
    auto remember_dangling = [&](VertexId v) {
      if (v >= vertex_count(g_)) {
        dangling_before.emplace_back(v, true);
        return;
      }
      dangling_before.emplace_back(v, neighbor_count(g_, v) == 0);
    };
    for (const auto& e : batch.updates) {
      remember_dangling(e.src);
      if (!is_directed(g_) && e.dst != e.src) remember_dangling(e.dst);
    }

    apply_updates(g_, batch);
    const auto n = vertex_count(g_);
    if (n == 0) {
      rank_.clear();
      last_repaired_vertices_ = 0;
      last_repair_iterations_ = 0;
      last_residual_l1_ = 0.0;
      last_residual_linf_ = 0.0;
      last_full_recompute_converged_ = true;
      return;
    }

    if (rank_.size() != n) rank_.resize(n, 1.0 / static_cast<double>(n));

    for (const auto& [v, was_dangling] : dangling_before) {
      const bool now_dangling = v >= n || neighbor_count(g_, v) == 0;
      if (was_dangling != now_dangling) {
        recompute();
        return;
      }
    }

    std::vector<std::uint8_t> active(n, 0);
    std::size_t active_count = 0;
    auto activate = [&](VertexId v) {
      if (v < n && !active[v]) {
        active[v] = 1;
        ++active_count;
      }
    };

    for (const auto& e : batch.updates) {
      activate(e.src);
      activate(e.dst);
      if (e.src < n) for_each_neighbor(g_, e.src, activate);
      if (!is_directed(g_) && e.dst < n) for_each_neighbor(g_, e.dst, activate);
    }

    if (active_count == 0) {
      last_repaired_vertices_ = 0;
      last_repair_iterations_ = 0;
      last_residual_l1_ = 0.0;
      last_residual_linf_ = 0.0;
      return;
    }

    const auto fallback_limit = static_cast<std::size_t>(
        std::max(1.0, full_fallback_fraction * static_cast<double>(n)));
    if (active_count >= fallback_limit) {
      recompute();
      return;
    }

    const double base = (1.0 - damping_) / static_cast<double>(n);
    std::vector<std::uint8_t> ever_active = active;
    last_repair_iterations_ = 0;
    last_residual_l1_ = 0.0;
    last_residual_linf_ = 0.0;
    last_full_recompute_converged_ = false;

    for (std::size_t it = 0; it < local_iterations && active_count != 0; ++it) {
      ++last_repair_iterations_;
      auto next_rank = rank_;
      std::vector<std::uint8_t> next_active(n, 0);
      std::size_t next_count = 0;
      double iter_l1 = 0.0;
      double iter_linf = 0.0;
      bool changed_dangling_rank = false;

      auto activate_next = [&](VertexId v) {
        if (v < n && !next_active[v]) {
          next_active[v] = 1;
          ++next_count;
          ever_active[v] = 1;
        }
      };

      for (VertexId v = 0; v < n; ++v) {
        if (!active[v]) continue;

        double incoming = 0.0;
        for_each_in_neighbor(g_, v, [&](VertexId u) {
          const auto out_degree = neighbor_count(g_, u);
          if (out_degree != 0) incoming += rank_[u] / static_cast<double>(out_degree);
        });

        const double updated = base + damping_ * incoming;
        const double delta = std::abs(updated - rank_[v]);
        next_rank[v] = updated;
        iter_l1 += delta;
        iter_linf = std::max(iter_linf, delta);

        if (delta > tol) {
          if (neighbor_count(g_, v) == 0) {
            changed_dangling_rank = true;
            break;
          }
          for_each_neighbor(g_, v, activate_next);
        }
      }

      if (changed_dangling_rank) {
        recompute();
        return;
      }

      rank_.swap(next_rank);
      active.swap(next_active);
      active_count = next_count;
      last_residual_l1_ = iter_l1;
      last_residual_linf_ = iter_linf;

      std::size_t repaired = 0;
      for (auto flag : ever_active) repaired += flag != 0;
      if (repaired >= fallback_limit) {
        recompute();
        return;
      }
    }

    last_repaired_vertices_ = 0;
    for (auto flag : ever_active) last_repaired_vertices_ += flag != 0;
  }

  void recompute(std::size_t max_iterations = 200, double tol = 1e-12) {
    const auto result = full_solve(max_iterations, tol);
    rank_ = result.values;
    last_repaired_vertices_ = vertex_count(g_);
    last_repair_iterations_ = result.iterations;
    last_residual_l1_ = result.residual_l1;
    last_residual_linf_ = result.residual_linf;
    last_full_recompute_converged_ = result.converged;
  }

  [[nodiscard]] PageRankValidation validate_against_full(
      std::size_t reference_max_iterations = 500,
      double reference_tol = 1e-12,
      double l1_tolerance = 1e-6,
      double linf_tolerance = 1e-7) const {
    const auto reference = full_solve(reference_max_iterations, reference_tol);
    PageRankValidation validation;
    validation.reference_iterations = reference.iterations;
    validation.reference_residual_l1 = reference.residual_l1;
    validation.reference_residual_linf = reference.residual_linf;
    validation.reference_converged = reference.converged;
    validation.local_residual_l1 = last_residual_l1_;
    validation.local_residual_linf = last_residual_linf_;

    const auto count = std::min(rank_.size(), reference.values.size());
    for (std::size_t i = 0; i < count; ++i) {
      const double error = std::abs(rank_[i] - reference.values[i]);
      validation.l1_error += error;
      validation.linf_error = std::max(validation.linf_error, error);
    }
    if (rank_.size() != reference.values.size()) {
      validation.l1_error = std::numeric_limits<double>::infinity();
      validation.linf_error = std::numeric_limits<double>::infinity();
    }
    validation.within_tolerance = reference.converged &&
                                  validation.l1_error <= l1_tolerance &&
                                  validation.linf_error <= linf_tolerance;
    return validation;
  }

  [[nodiscard]] PageRankValidation apply_validated(
      const UpdateBatch& batch,
      std::size_t local_iterations = 64,
      double local_tol = 1e-10,
      double full_fallback_fraction = 0.95,
      std::size_t reference_max_iterations = 500,
      double reference_tol = 1e-12,
      double l1_tolerance = 1e-6,
      double linf_tolerance = 1e-7) {
    apply(batch, local_iterations, local_tol, full_fallback_fraction);
    auto validation = validate_against_full(reference_max_iterations, reference_tol,
                                            l1_tolerance, linf_tolerance);
    if (!validation.within_tolerance) {
      const auto reference = full_solve(reference_max_iterations, reference_tol);
      rank_ = reference.values;
      last_repaired_vertices_ = vertex_count(g_);
      last_repair_iterations_ = reference.iterations;
      last_residual_l1_ = reference.residual_l1;
      last_residual_linf_ = reference.residual_linf;
      last_full_recompute_converged_ = reference.converged;
      validation.fallback_applied = true;
    }
    return validation;
  }

 private:
  struct FullSolveResult {
    std::vector<double> values;
    std::size_t iterations{0};
    double residual_l1{0.0};
    double residual_linf{0.0};
    bool converged{false};
  };

  [[nodiscard]] FullSolveResult full_solve(std::size_t max_iterations, double tol) const {
    FullSolveResult result;
    const auto n = vertex_count(g_);
    if (n == 0) {
      result.converged = true;
      return result;
    }

    result.values.assign(n, 1.0 / static_cast<double>(n));
    std::vector<std::size_t> out_degree(n, 0);
    for (VertexId u = 0; u < n; ++u) out_degree[u] = neighbor_count(g_, u);

    const double teleport = (1.0 - damping_) / static_cast<double>(n);
    for (std::size_t it = 0; it < max_iterations; ++it) {
      double dangling_mass = 0.0;
      for (VertexId u = 0; u < n; ++u) {
        if (out_degree[u] == 0) dangling_mass += result.values[u];
      }
      const double dangling_share = damping_ * dangling_mass / static_cast<double>(n);

      std::vector<double> next(n, teleport + dangling_share);
      for (VertexId v = 0; v < n; ++v) {
        double incoming = 0.0;
        for_each_in_neighbor(g_, v, [&](VertexId u) {
          if (out_degree[u] != 0) incoming += result.values[u] / static_cast<double>(out_degree[u]);
        });
        next[v] += damping_ * incoming;
      }

      result.residual_l1 = 0.0;
      result.residual_linf = 0.0;
      for (std::size_t i = 0; i < n; ++i) {
        const double delta = std::abs(next[i] - result.values[i]);
        result.residual_l1 += delta;
        result.residual_linf = std::max(result.residual_linf, delta);
      }
      result.values.swap(next);
      result.iterations = it + 1;
      if (result.residual_linf <= tol) {
        result.converged = true;
        break;
      }
    }
    return result;
  }

  Graph& g_;
  double damping_;
  std::vector<double> rank_;
  std::size_t last_repaired_vertices_{0};
  std::size_t last_repair_iterations_{0};
  double last_residual_l1_{0.0};
  double last_residual_linf_{0.0};
  bool last_full_recompute_converged_{false};
};

using IncrementalPageRank = BasicIncrementalPageRank<DynamicGraph>;

}  // namespace velographx
