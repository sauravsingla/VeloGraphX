#!/usr/bin/env python3
"""Build a v3 cascade selector that reuses IncrementalBFS preflight work.

The candidate keeps the current reach/full-cost correction, asks IncrementalBFS to
perform bounded deletion-cascade discovery before repair, and commits the prepared
workspace when incremental execution is selected. If the bound is exceeded, the
selector discards the prepared state and owns a fresh recomputation. No H6 input is
read by this transform.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from prepare_bfs_selector_v3_candidate import add_reachfix, replace_once


def add_reusable_cascade(src: str) -> str:
    trace_anchor = "  double normalized_tail_ratio{1.0};\n"
    trace_extra = (
        trace_anchor
        + "  double cascade_preview_fraction{0.0};\n"
        + "  std::size_t cascade_preview_vertices{0};\n"
        + "  std::size_t cascade_preview_candidates{0};\n"
        + "  bool cascade_preview_evaluated{false};\n"
        + "  bool cascade_preview_exceeded{false};\n"
        + "  bool cascade_preflight_reused{false};\n"
    )
    src = replace_once(src, trace_anchor, trace_extra, "trace telemetry")

    signal_anchor = "    t.structural_change_fraction = signals.structural_change_fraction;\n\n    bool choose_full = false;\n"
    signal_replacement = (
        "    t.structural_change_fraction = signals.structural_change_fraction;\n\n"
        "    const bool deletion_pressure =\n"
        "        signals.shortest_parent_deletion_fraction >= kAffectedBudget;\n"
        "    velographx::IncrementalBFS::DeletionCascadePreflight cascade;\n"
        "    if (t.update_fraction < kPreflightFullUpdate && deletion_pressure) {\n"
        "      cascade = bfs.preflight_deletion_cascade(updates, 3.0 * kAffectedBudget);\n"
        "    }\n"
        "    t.cascade_preview_vertices = cascade.affected_vertices;\n"
        "    t.cascade_preview_candidates = cascade.deletion_candidates;\n"
        "    t.cascade_preview_fraction = static_cast<double>(cascade.affected_vertices) /\n"
        "        static_cast<double>(std::max<std::size_t>(1, vertices));\n"
        "    t.cascade_preview_evaluated = cascade.prepared;\n"
        "    t.cascade_preview_exceeded = cascade.exceeded;\n\n"
        "    bool choose_full = false;\n"
    )
    src = replace_once(src, signal_anchor, signal_replacement, "reusable cascade telemetry")

    branch_anchor = "    if (t.update_fraction >= kPreflightFullUpdate) {\n"
    branch_replacement = (
        "    if (cascade.exceeded) {\n"
        "      choose_full = true;\n"
        "      t.reason = \"v3_reuse_cascade_preflight_full\";\n"
        "    } else if (t.update_fraction >= kPreflightFullUpdate) {\n"
    )
    src = replace_once(src, branch_anchor, branch_replacement, "cascade full guard")

    # Cost-only full selection must have deletion-side structural evidence. This
    # prevents a stale/noisy cost estimate from manufacturing a full decision.
    fresh_anchor = (
        "        const bool fresh_expected_full = full_age < kFreshAge &&\n"
        "            t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;\n"
    )
    fresh_replacement = (
        "        const bool fresh_expected_full = full_age < kFreshAge && deletion_pressure &&\n"
        "            t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;\n"
    )
    src = replace_once(src, fresh_anchor, fresh_replacement, "structural evidence for fresh full")

    exec_anchor = '''    if (choose_full) {
      graph.apply(updates);
      bfs.recompute();
      ++r.full_recompute_batches;
    } else {
      bfs.apply(updates);
      r.affected_vertices += bfs.last_affected_vertices();
      fallback = bfs.last_used_full_recompute();
'''
    exec_replacement = '''    if (choose_full) {
      if (cascade.prepared) bfs.discard_deletion_preflight();
      graph.apply(updates);
      bfs.recompute();
      ++r.full_recompute_batches;
    } else {
      bool reused_preflight = false;
      if (cascade.prepared) reused_preflight = bfs.apply_preflighted(updates);
      if (!reused_preflight) bfs.apply(updates);
      t.cascade_preflight_reused = reused_preflight;
      r.affected_vertices += bfs.last_affected_vertices();
      fallback = bfs.last_used_full_recompute();
'''
    src = replace_once(src, exec_anchor, exec_replacement, "reuse prepared execution")

    serial_anchor = '              << ",\\\"normalized_tail_ratio\\\":" << t.normalized_tail_ratio\n'
    serial_replacement = (
        serial_anchor
        + '              << ",\\\"cascade_preview_fraction\\\":" << t.cascade_preview_fraction\n'
        + '              << ",\\\"cascade_preview_vertices\\\":" << t.cascade_preview_vertices\n'
        + '              << ",\\\"cascade_preview_candidates\\\":" << t.cascade_preview_candidates\n'
        + '              << ",\\\"cascade_preview_evaluated\\\":" << (t.cascade_preview_evaluated ? "true" : "false")\n'
        + '              << ",\\\"cascade_preview_exceeded\\\":" << (t.cascade_preview_exceeded ? "true" : "false")\n'
        + '              << ",\\\"cascade_preflight_reused\\\":" << (t.cascade_preflight_reused ? "true" : "false")\n'
    )
    src = replace_once(src, serial_anchor, serial_replacement, "cascade serialization")

    src = replace_once(
        src,
        '\\"selector\\":\\"publication-preflight-bfs-v3-light3\\"',
        '\\"selector\\":\\"publication-preflight-bfs-v3-cascade-reuse1\\"',
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
        '\\"selector_change\\":\\"reusable bounded pre-repair deletion-cascade discovery; selector-owned recomputation above 3x affected-work budget; prepared discovery is committed without duplicate work\\"',
        "selector change",
    )
    return src


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    reachfixed = add_reachfix(args.input.read_text())
    candidate = add_reusable_cascade(reachfixed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(candidate)


if __name__ == "__main__":
    main()
