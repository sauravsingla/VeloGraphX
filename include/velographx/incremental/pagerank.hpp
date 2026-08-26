#pragma once
#include <cmath>
#include <vector>
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {
class IncrementalPageRank {
 public:
  explicit IncrementalPageRank(DynamicGraph& g, double damping = 0.85) : g_(g), damping_(damping) { recompute(); }
  [[nodiscard]] const std::vector<double>& values() const noexcept { return rank_; }
  void apply(const UpdateBatch& batch, std::size_t local_iterations = 12, double tol = 1e-9) {
    g_.apply(batch);
    if (rank_.size() != g_.vertex_count()) rank_.resize(g_.vertex_count(), 1.0 / std::max<std::size_t>(1,g_.vertex_count()));
    std::vector<char> active(g_.vertex_count(), 0);
    for (const auto& e : batch.updates) { active[e.src] = 1; active[e.dst] = 1; for (auto v : g_.neighbors(e.src)) active[v] = 1; }
    const double base = (1.0 - damping_) / std::max<std::size_t>(1, g_.vertex_count());
    for (std::size_t it = 0; it < local_iterations; ++it) {
      double max_delta = 0;
      auto next = rank_;
      for (VertexId v = 0; v < g_.vertex_count(); ++v) if (active[v]) {
        double incoming = 0;
        for (VertexId u = 0; u < g_.vertex_count(); ++u) {
          auto n = g_.neighbors(u);
          if (!n.empty() && std::binary_search(n.begin(), n.end(), v)) incoming += rank_[u] / n.size();
        }
        next[v] = base + damping_ * incoming;
        max_delta = std::max(max_delta, std::abs(next[v]-rank_[v]));
      }
      rank_.swap(next);
      if (max_delta < tol) break;
    }
  }
  void recompute(std::size_t iterations = 30) {
    const auto n = std::max<std::size_t>(1, g_.vertex_count());
    rank_.assign(n, 1.0 / n);
    const double base = (1.0 - damping_) / n;
    for (std::size_t it = 0; it < iterations; ++it) {
      std::vector<double> next(n, base);
      for (VertexId u = 0; u < g_.vertex_count(); ++u) {
        auto ns = g_.neighbors(u);
        if (ns.empty()) continue;
        const auto share = damping_ * rank_[u] / ns.size();
        for (auto v : ns) next[v] += share;
      }
      rank_.swap(next);
    }
  }
 private:
  DynamicGraph& g_;
  double damping_;
  std::vector<double> rank_;
};
} // namespace velographx
