#!/usr/bin/env python3
"""Build the v3 cascade-risk development candidate from the current reach-fix source.

This is intentionally a development-only source transform. It uses only already-seen
road/H4 evidence and never reads H6. The added preflight probe is bounded: it follows
shortest-path dependency loss only until 3 * kAffectedBudget of vertices are exposed.
If the cap is crossed, the selector owns recomputation before IncrementalBFS repair is
started. Probe time stays inside selector decision timing.
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
    src = replace_once(
        src,
        "#include <numeric>\n",
        "#include <numeric>\n#include <unordered_map>\n#include <unordered_set>\n",
        "include insertion",
    )

    trace_anchor = "  double normalized_tail_ratio{1.0};\n"
    trace_extra = (
        trace_anchor
        + "  double cascade_preview_fraction{0.0};\n"
        + "  std::size_t cascade_preview_vertices{0};\n"
        + "  std::size_t cascade_preview_scanned_edges{0};\n"
        + "  bool cascade_preview_evaluated{false};\n"
        + "  bool cascade_preview_exceeded{false};\n"
    )
    src = replace_once(src, trace_anchor, trace_extra, "trace telemetry")

    helper_anchor = "L3Result run_l3_baseline(const std::string& policy,\n"
    helper = r'''struct L3CascadePreview {
  bool evaluated{false};
  bool exceeded{false};
  std::size_t affected_vertices{0};
  std::size_t scanned_edges{0};
  double affected_fraction{0.0};
};

L3CascadePreview l3_cascade_preview(const velographx::DynamicGraph& graph,
                                    const velographx::UpdateBatch& updates,
                                    const std::vector<std::uint32_t>& dist,
                                    double stop_fraction) {
  L3CascadePreview out;
  out.evaluated = true;
  const auto unreachable = std::numeric_limits<std::uint32_t>::max();
  const std::size_t vertices = graph.vertex_count();
  const std::size_t stop_vertices = std::max<std::size_t>(
      1, static_cast<std::size_t>(std::ceil(
             stop_fraction * static_cast<double>(std::max<std::size_t>(1, vertices))))));

  auto edge_key = [](velographx::VertexId u, velographx::VertexId v) {
    return (static_cast<std::uint64_t>(u) << 32) | static_cast<std::uint64_t>(v);
  };

  std::unordered_set<std::uint64_t> seen_final;
  std::unordered_set<std::uint64_t> final_deletions;
  std::vector<std::pair<velographx::VertexId, velographx::VertexId>> deletions;
  seen_final.reserve(updates.updates.size() * 2 + 1);
  final_deletions.reserve(updates.updates.size() + 1);
  deletions.reserve(updates.updates.size());

  for (auto it = updates.updates.rbegin(); it != updates.updates.rend(); ++it) {
    const auto key = edge_key(it->src, it->dst);
    if (!seen_final.insert(key).second) continue;
    if (!it->add && velographx::has_edge(graph, it->src, it->dst)) {
      deletions.emplace_back(it->src, it->dst);
      final_deletions.insert(key);
    }
  }

  std::unordered_map<velographx::VertexId, std::size_t> lost_support;
  std::unordered_map<velographx::VertexId, std::size_t> support_cache;
  std::unordered_set<velographx::VertexId> affected;
  std::vector<velographx::VertexId> queue;
  lost_support.reserve(deletions.size() * 2 + 1);
  support_cache.reserve(deletions.size() * 4 + 1);
  affected.reserve(stop_vertices * 2 + 1);
  queue.reserve(std::min<std::size_t>(stop_vertices + 1, vertices));

  auto shortest_support = [&](velographx::VertexId v) -> std::size_t {
    const auto found = support_cache.find(v);
    if (found != support_cache.end()) return found->second;
    std::size_t count = 0;
    if (v < dist.size() && dist[v] != unreachable) {
      velographx::for_each_in_neighbor(graph, v, [&](velographx::VertexId p) {
        ++out.scanned_edges;
        if (p < dist.size() && dist[p] != unreachable && dist[p] + 1 == dist[v]) ++count;
      });
    }
    support_cache.emplace(v, count);
    return count;
  };

  for (const auto& [u, v] : deletions) {
    if (u >= dist.size() || v >= dist.size() ||
        dist[u] == unreachable || dist[v] == unreachable || dist[u] + 1 != dist[v]) {
      continue;
    }
    ++lost_support[v];
  }

  auto mark_if_unsupported = [&](velographx::VertexId v) {
    if (affected.contains(v)) return;
    const std::size_t support = shortest_support(v);
    const auto lost = lost_support.find(v);
    if (support != 0 && lost != lost_support.end() && lost->second >= support) {
      affected.insert(v);
      queue.push_back(v);
    }
  };
  for (const auto& [v, ignored] : lost_support) {
    (void)ignored;
    mark_if_unsupported(v);
  }

  std::size_t head = 0;
  while (head < queue.size() && queue.size() <= stop_vertices) {
    const auto u = queue[head++];
    if (u >= dist.size() || dist[u] == unreachable) continue;
    velographx::for_each_neighbor(graph, u, [&](velographx::VertexId v) {
      ++out.scanned_edges;
      if (queue.size() > stop_vertices || v >= dist.size() || dist[v] != dist[u] + 1) return;
      if (final_deletions.contains(edge_key(u, v))) return;
      if (affected.contains(v)) return;
      ++lost_support[v];
      mark_if_unsupported(v);
    });
  }

  out.affected_vertices = queue.size();
  out.affected_fraction = static_cast<double>(out.affected_vertices) /
      static_cast<double>(std::max<std::size_t>(1, vertices));
  out.exceeded = out.affected_vertices > stop_vertices;
  return out;
}

'''
    src = replace_once(src, helper_anchor, helper + helper_anchor, "cascade helper")

    signal_anchor = "    t.structural_change_fraction = signals.structural_change_fraction;\n\n    bool choose_full = false;\n"
    signal_replacement = (
        "    t.structural_change_fraction = signals.structural_change_fraction;\n\n"
        "    const bool reachability_churn =\n"
        "        signals.unreachable_target_insertion_fraction >= kAffectedBudget;\n"
        "    L3CascadePreview cascade;\n"
        "    if (t.update_fraction < kPreflightFullUpdate && reachability_churn) {\n"
        "      cascade = l3_cascade_preview(graph, updates, bfs.distances(),\n"
        "                                   3.0 * kAffectedBudget);\n"
        "    }\n"
        "    t.cascade_preview_fraction = cascade.affected_fraction;\n"
        "    t.cascade_preview_vertices = cascade.affected_vertices;\n"
        "    t.cascade_preview_scanned_edges = cascade.scanned_edges;\n"
        "    t.cascade_preview_evaluated = cascade.evaluated;\n"
        "    t.cascade_preview_exceeded = cascade.exceeded;\n\n"
        "    bool choose_full = false;\n"
    )
    src = replace_once(src, signal_anchor, signal_replacement, "cascade decision telemetry")

    branch_anchor = "    if (t.update_fraction >= kPreflightFullUpdate) {\n"
    branch_replacement = (
        "    if (cascade.exceeded) {\n"
        "      choose_full = true;\n"
        "      t.reason = \"v3_cascade_preflight_full\";\n"
        "    } else if (t.update_fraction >= kPreflightFullUpdate) {\n"
    )
    src = replace_once(src, branch_anchor, branch_replacement, "cascade full guard")

    fresh_anchor = (
        "        const bool fresh_expected_full = full_age < kFreshAge &&\n"
        "            t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;\n"
    )
    fresh_replacement = (
        "        const bool fresh_expected_full = full_age < kFreshAge && reachability_churn &&\n"
        "            t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;\n"
    )
    src = replace_once(src, fresh_anchor, fresh_replacement, "structural evidence for fresh full")

    serial_anchor = (
        '              << ",\\\"normalized_tail_ratio\\\":" << t.normalized_tail_ratio\n'
    )
    serial_replacement = (
        serial_anchor
        + '              << ",\\\"cascade_preview_fraction\\\":" << t.cascade_preview_fraction\n'
        + '              << ",\\\"cascade_preview_vertices\\\":" << t.cascade_preview_vertices\n'
        + '              << ",\\\"cascade_preview_scanned_edges\\\":" << t.cascade_preview_scanned_edges\n'
        + '              << ",\\\"cascade_preview_evaluated\\\":" << (t.cascade_preview_evaluated ? "true" : "false")\n'
        + '              << ",\\\"cascade_preview_exceeded\\\":" << (t.cascade_preview_exceeded ? "true" : "false")\n'
    )
    src = replace_once(src, serial_anchor, serial_replacement, "cascade serialization")

    src = replace_once(
        src,
        '\\"selector\\":\\"publication-preflight-bfs-v3-light3\\"',
        '\\"selector\\":\\"publication-preflight-bfs-v3-cascade1\\"',
        "selector id",
    )
    src = replace_once(
        src,
        '\\"parent_selector\\":\\"publication-preflight-bfs-v3-light2\\"',
        '\\"parent_selector\\":\\"publication-preflight-bfs-v3-light3\\"',
        "parent selector",
    )
    src = replace_once(
        src,
        '\\"selector_change\\":\\"affected-work-discounted robust incremental learning plus one-probe normalized residual tail guard; no forced stale-full refresh\\"',
        '\\"selector_change\\":\\"bounded pre-repair shortest-parent cascade preview under reachability churn; selector-owned full when preview exceeds 3x affected-work budget; fresh cost-only full requires structural churn\\"',
        "selector change",
    )
    return src


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.input.read_text()
    out = transform(source)
    if "publication-preflight-bfs-v3-cascade1" not in out:
        raise RuntimeError("cascade selector id missing after transform")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(out)


if __name__ == "__main__":
    main()
