#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {

class IncrementalPageRank {
 public:
  explicit IncrementalPageRank(DynamicGraph& g, double damping = 0.85)
      : g_(g), damping_(damping) {
    recompute();
  }

  [[nodiscard]] const std::vector<double>& values() const noexcept { return rank_; }
  [[nodiscard]] std::size_t last_repaired_vertices() const noexcept {
    return last_repaired_vertices_;
  }
  [[nodiscard]] std::size_t last_repair_iterations() const noexcept {
    return last_repair_iterations_;
  }

  void apply(const UpdateBatch& batch, std::size_t local_iterations = 24,
             double tol = 1e-9, double full_fallback_fraction = 0.60) {
    g_.apply(batch);
    const auto n = g_.vertex_count();
    if (n == 0) {
      rank_.clear();
      last_repaired_vertices_ = 0;
      last_repair_iterations_ = 0;
      return;
    }

    if (rank_.size() != n) rank_.resize(n, 1.0 / static_cast<double>(n));

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
      if (e.src < n) {
        for (auto v : g_.neighbors(e.src)) activate(v);
      }
      if (!g_.directed() && e.dst < n) {
        for (auto v : g_.neighbors(e.dst)) activate(v);
      }
    }

    if (active_count == 0) {
      last_repaired_vertices_ = 0;
      last_repair_iterations_ = 0;
      return;
    }

    const auto fallback_limit = static_cast<std::size_t>(
        std::max(1.0, full_fallback_fraction * static_cast<double>(n)));
    if (active_count >= fallback_limit) {
      recompute();
      last_repaired_vertices_ = n;
      return;
    }

    const double base = (1.0 - damping_) / static_cast<double>(n);
    std::vector<std::uint8_t> ever_active = active;
    last_repair_iterations_ = 0;

    for (std::size_t it = 0; it < local_iterations && active_count != 0; ++it) {
      ++last_repair_iterations_;
      auto next_rank = rank_;
      std::vector<std::uint8_t> next_active(n, 0);
      std::size_t next_count = 0;

      auto activate_next = [&](VertexId v) {
        if (v < n && !next_active[v]) {
          next_active[v] = 1;
          ++next_count;
          ever_active[v] = 1;
        }
      };

      for (VertexId v = 0; v < n; ++v) {
        if (!active[v]) continue;

        // Reverse adjacency makes localized repair proportional to the actual
        // predecessor set instead of scanning every vertex in the graph.
        double incoming = 0.0;
        for (auto u : g_.in_neighbors(v)) {
          const auto out_degree = g_.neighbors(u).size();
          if (out_degree != 0) {
            incoming += rank_[u] / static_cast<double>(out_degree);
          }
        }

        const double updated = base + damping_ * incoming;
        const double delta = std::abs(updated - rank_[v]);
        next_rank[v] = updated;

        if (delta > tol) {
          for (auto dst : g_.neighbors(v)) activate_next(dst);
        }
      }

      rank_.swap(next_rank);
      active.swap(next_active);
      active_count = next_count;

      std::size_t repaired = 0;
      for (auto flag : ever_active) repaired += flag != 0;
      if (repaired >= fallback_limit) {
        recompute();
        last_repaired_vertices_ = n;
        return;
      }
    }

    last_repaired_vertices_ = 0;
    for (auto flag : ever_active) last_repaired_vertices_ += flag != 0;
  }

  void recompute(std::size_t iterations = 30) {
    const auto vertices = g_.vertex_count();
    if (vertices == 0) {
      rank_.clear();
      last_repaired_vertices_ = 0;
      last_repair_iterations_ = 0;
      return;
    }

    const auto n = vertices;
    rank_.assign(n, 1.0 / static_cast<double>(n));
    const double base = (1.0 - damping_) / static_cast<double>(n);
    for (std::size_t it = 0; it < iterations; ++it) {
      std::vector<double> next(n, base);
      for (VertexId u = 0; u < n; ++u) {
        auto neighbors = g_.neighbors(u);
        if (neighbors.empty()) continue;
        const auto share = damping_ * rank_[u] / static_cast<double>(neighbors.size());
        for (auto v : neighbors) next[v] += share;
      }
      rank_.swap(next);
    }
    last_repaired_vertices_ = n;
    last_repair_iterations_ = iterations;
  }

 private:
  DynamicGraph& g_;
  double damping_;
  std::vector<double> rank_;
  std::size_t last_repaired_vertices_{0};
  std::size_t last_repair_iterations_{0};
};

}  // namespace velographx
