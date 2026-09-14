#!/usr/bin/env python3
"""Build v3 cascade-reuse2 for seen-development validation only.

Changes from cascade-reuse1 are deliberately small and auditable:
1. stop bounded deletion-cascade discovery at 2 * kAffectedBudget rather than
   3 * kAffectedBudget; once repair dependency loss already spans twice the
   selector's nominal affected-work budget, recomputation owns the batch;
2. remove the independent fresh expected-cost full trigger. Pre-repair cascade
   evidence, the existing conservative uncertainty test, or the existing hard
   preflight rule must justify a full decision.

The preflight workspace is still reused when repair is selected, so discovery is
not paid twice. This transform consumes only already-seen road/H4 development data;
it never reads or generates H6.
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
        "      cascade = bfs.preflight_deletion_cascade(updates, 2.0 * kAffectedBudget);\n"
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
        "      t.reason = \"v3_reuse2_cascade_preflight_full\";\n"
        "    } else if (t.update_fraction >= kPreflightFullUpdate) {\n"
    )
    src = replace_once(src, branch_anchor, branch_replacement, "cascade full guard")

    # The prior fresh expected-cost trigger produced full decisions without a
    # sufficiently strong pre-repair risk signal. Disable that independent arm;
    # the uncertainty-confidence and tail guards remain unchanged.
    fresh_anchor = (
        "        const bool fresh_expected_full = full_age < kFreshAge &&\n"
        "            t.predicted_incremental_us > t.predicted_full_us * kLearnedFullMargin;\n"
    )
    fresh_replacement = "        const bool fresh_expected_full = false;\n"
    src = replace_once(src, fresh_anchor, fresh_replacement, "disable cost-only fresh full")

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
    adaptive_start = src.index("L3Result run_adaptive_l3")
    prefix = src[:adaptive_start]
    adaptive = src[adaptive_start:]
    adaptive = replace_once(
        adaptive,
        exec_anchor,
        exec_replacement,
        "reuse prepared adaptive execution",
    )
    src = prefix + adaptive

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
        '\\"selector\\":\\"publication-preflight-bfs-v3-cascade-reuse2\\"',
        "selector id",
    )
    src = replace_once(
        src,
        '\\"parent_selector\\":\\"publication-preflight-bfs-v3-light2\\"',
        '\\"parent_selector\\":\\"publication-preflight-bfs-v3-cascade-reuse1\\"',
        "parent selector",
    )
    src = replace_once(
        src,
        '\\"selector_change\\":\\"affected-work-discounted robust incremental learning plus one-probe normalized residual tail guard; no forced stale-full refresh\\"',
        '\\"selector_change\\":\\"reusable bounded deletion-cascade preflight stopped at 2x affected-work budget; independent fresh expected-cost full trigger disabled; no duplicate affected-region discovery\\"',
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
