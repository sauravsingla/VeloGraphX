#!/usr/bin/env python3
"""Generate a development-only IncrementalBFS header with reusable deletion preflight.

The generated header preserves the existing public apply() path. It adds a bounded
pre-repair deletion-cascade discovery API whose prepared workspace can be committed
without repeating affected-region discovery, or discarded before selector-owned full
recomputation. This keeps preflight work inside selector timing without double work.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


def transform(src: str) -> str:
    public_anchor = "  void apply(const UpdateBatch& batch) {\n"
    public_methods = r'''  struct DeletionCascadePreflight {
    bool prepared{false};
    bool exceeded{false};
    std::size_t affected_vertices{0};
    std::size_t deletion_candidates{0};
  };

  [[nodiscard]] DeletionCascadePreflight preflight_deletion_cascade(
      const UpdateBatch& batch, double affected_fraction_limit) {
    if (preflight_ready_) discard_deletion_preflight();
    last_deletion_candidates_ = 0;
    last_affected_vertices_ = 0;
    last_used_full_recompute_ = false;
    if (batch.empty()) return {};

    ensure_workspace(vertex_count(g_));
    prepare_batch_workspace(batch.updates.size());

    for (auto it = batch.updates.rbegin(); it != batch.updates.rend(); ++it) {
      auto u = it->src;
      auto v = it->dst;
      if (!is_directed(g_) && v < u) std::swap(u, v);
      const auto key = edge_key(u, v);
      if (!seen_final_updates_.insert(key)) continue;
      if (it->add) {
        final_additions_.emplace_back(it->src, it->dst);
      } else {
        final_deletions_.emplace_back(it->src, it->dst);
        final_deletion_keys_.insert(key);
      }
    }

    deletion_candidates_.reserve(final_deletions_.size() * (is_directed(g_) ? 1 : 2));
    existing_deletions_.reserve(final_deletions_.size());
    for (const auto& [u, v] : final_deletions_) {
      if (!has_edge(g_, u, v)) continue;
      existing_deletions_.emplace_back(u, v);
      if (is_shortest_parent(u, v)) deletion_candidates_.push_back(v);
      if (!is_directed(g_) && is_shortest_parent(v, u)) deletion_candidates_.push_back(u);
    }
    std::sort(deletion_candidates_.begin(), deletion_candidates_.end());
    deletion_candidates_.erase(std::unique(deletion_candidates_.begin(), deletion_candidates_.end()),
                               deletion_candidates_.end());
    last_deletion_candidates_ = deletion_candidates_.size();

    affected_vertices_.reserve(std::min<std::size_t>(deletion_candidates_.size() * 2 + 8,
                                                      vertex_count(g_)));
    bool within_limit = true;
    if (!deletion_candidates_.empty()) {
      within_limit = compute_affected_prebatch_limit(
          existing_deletions_, final_deletion_keys_, affected_fraction_limit);
    }
    preflight_ready_ = true;
    preflight_exceeded_ = !within_limit;
    return {true, preflight_exceeded_, last_affected_vertices_, last_deletion_candidates_};
  }

  [[nodiscard]] bool apply_preflighted(const UpdateBatch& batch) {
    if (!preflight_ready_ || preflight_exceeded_) return false;

    apply_updates(g_, batch);
    if (dist_.size() < vertex_count(g_)) dist_.resize(vertex_count(g_), unreachable);
    ensure_workspace(vertex_count(g_));

    if (!affected_vertices_.empty()) {
      old_affected_dist_.reserve(affected_vertices_.size());
      for (auto v : affected_vertices_) {
        old_affected_dist_.push_back(v < dist_.size() ? dist_[v] : unreachable);
      }
      repair_affected();
    }

    bfs_queue_.clear();
    for (const auto& [u, v] : final_additions_) {
      relax_edge(u, v, bfs_queue_);
      if (!is_directed(g_)) relax_edge(v, u, bfs_queue_);
    }
    propagate_decreases(bfs_queue_);
    clear_workspace();
    preflight_ready_ = false;
    preflight_exceeded_ = false;
    return true;
  }

  void discard_deletion_preflight() {
    if (preflight_ready_) clear_workspace();
    preflight_ready_ = false;
    preflight_exceeded_ = false;
  }

'''
    src = replace_once(src, public_anchor, public_methods + public_anchor, "public preflight API")

    affected_anchor = '''  bool compute_affected_prebatch(
      const std::vector<std::pair<VertexId, VertexId>>& existing_deletions,
      const ReusableKeySet& final_deletion_keys) {
    const auto fallback_limit = std::max<std::size_t>(
        1, static_cast<std::size_t>(static_cast<double>(vertex_count(g_)) * deletion_fallback_fraction_));
'''
    affected_replacement = '''  bool compute_affected_prebatch(
      const std::vector<std::pair<VertexId, VertexId>>& existing_deletions,
      const ReusableKeySet& final_deletion_keys) {
    return compute_affected_prebatch_limit(
        existing_deletions, final_deletion_keys, deletion_fallback_fraction_);
  }

  bool compute_affected_prebatch_limit(
      const std::vector<std::pair<VertexId, VertexId>>& existing_deletions,
      const ReusableKeySet& final_deletion_keys,
      double fallback_fraction) {
    const auto fallback_limit = std::max<std::size_t>(
        1, static_cast<std::size_t>(static_cast<double>(vertex_count(g_)) * fallback_fraction));
'''
    src = replace_once(src, affected_anchor, affected_replacement, "bounded affected discovery")

    state_anchor = "  bool last_used_full_recompute_{false};\n"
    state_replacement = (
        state_anchor
        + "  bool preflight_ready_{false};\n"
        + "  bool preflight_exceeded_{false};\n"
    )
    src = replace_once(src, state_anchor, state_replacement, "preflight state")
    return src


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    out = transform(args.input.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(out)


if __name__ == "__main__":
    main()
