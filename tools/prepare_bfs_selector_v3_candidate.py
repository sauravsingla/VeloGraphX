#!/usr/bin/env python3
"""Compose the current-reach fix with the bounded cascade-risk v3 candidate."""

from __future__ import annotations

import argparse
from pathlib import Path

from prepare_bfs_selector_v3_cascade import transform as add_cascade


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


def add_reachfix(src: str) -> str:
    # Preserve the road-safe confirmation behavior already established on the
    # seen development sets.
    anchor = "if (confirmation_batch) {"
    terminator = "} else if (normalized_tail >= kLearnedFullMargin) {"
    reset_needle = "        incremental_tail_ratio = std::max(decayed_tail, normalized_tail);"
    reset_replacement = "        incremental_tail_ratio = normalized_tail;"
    if src.count(anchor) != 1:
        raise RuntimeError("confirmation branch count changed unexpectedly")
    start = src.index(anchor)
    end = src.index(terminator, start)
    block = src[start:end]
    if block.count(reset_needle) != 1:
        raise RuntimeError("confirmation assignment missing or ambiguous")
    block = block.replace(reset_needle, reset_replacement)
    out = src[:start] + block + src[end:]
    if out.count(reset_replacement) != 1 or out[end:].count(reset_needle) < 1:
        raise RuntimeError("confirmation reset transform failed")

    # Use the maintained exact O(1) reachable count rather than rescanning the
    # distance vector; scale the last measured full cost by current reachable work.
    out = replace_once(
        out,
        "reachable_vertices(bfs.distances())",
        "bfs.reachable_count()",
        "adaptive reachability scan",
    )
    out = replace_once(
        out,
        "  double latest_full_us = initial_full_us;\n",
        "  double latest_full_us = initial_full_us;\n"
        "  std::size_t last_full_reachable_count = bfs.reachable_count();\n",
        "full-cost state",
    )
    out = replace_once(
        out,
        "    t.reachable_fraction = reachable_fraction;",
        "    const std::size_t current_reachable_count = bfs.reachable_count();\n"
        "    t.reachable_fraction = static_cast<double>(current_reachable_count) /\n"
        "        static_cast<double>(std::max<std::size_t>(1, vertices));",
        "per-batch reachability trace",
    )
    out = replace_once(
        out,
        "      t.predicted_full_us = latest_full_us;",
        "      const double reachable_work_ratio =\n"
        "          static_cast<double>(current_reachable_count) /\n"
        "          static_cast<double>(std::max<std::size_t>(1, last_full_reachable_count));\n"
        "      t.predicted_full_us = latest_full_us * reachable_work_ratio;",
        "full prediction",
    )
    out = replace_once(
        out,
        "      latest_full_us = execution_us;\n",
        "      latest_full_us = execution_us;\n"
        "      last_full_reachable_count = bfs.reachable_count();\n",
        "full observation",
    )
    adaptive = out[out.index("L3Result run_adaptive_l3"):]
    if "reachable_vertices(bfs.distances())" in adaptive:
        raise RuntimeError("adaptive selector still rescans distances")
    if adaptive.count("bfs.reachable_count()") < 4:
        raise RuntimeError("reachable-count transform incomplete")
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    reachfixed = add_reachfix(args.input.read_text())
    candidate = add_cascade(reachfixed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(candidate)


if __name__ == "__main__":
    main()
