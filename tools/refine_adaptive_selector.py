#!/usr/bin/env python3
from pathlib import Path
import argparse


def replace_once(text: str, old: str, new: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one match, found {count}: {old[:80]!r}")
    return text.replace(old, new, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    text = Path(args.input).read_text()

    text = replace_once(
        text,
        "  graph.bulk_load_edges(initial);\n  const auto initial_bfs_begin = Clock::now();\n  const double repair_budget =\n      policy == \"always_incremental\" ? 2.0 :\n      ((policy == \"adaptive\" && vertices >= 200000) ? 2.0 : kAffectedBudget);",
        "  graph.bulk_load_edges(initial);\n"
        "  std::size_t root_degree = 0;\n"
        "  std::size_t root_in_degree = 0;\n"
        "  if (root < graph.vertex_count()) {\n"
        "    graph.for_each_neighbor(root, [&](velographx::VertexId) { ++root_degree; });\n"
        "    graph.for_each_in_neighbor(root, [&](velographx::VertexId) { ++root_in_degree; });\n"
        "  }\n"
        "  const double root_sink_ratio = static_cast<double>(root_in_degree + 1) /\n"
        "      static_cast<double>(root_degree + 1);\n"
        "  // Low-locality roots on large graphs can trigger very large repair regions.\n"
        "  // Keep the normal selector-owned budget for well-connected roots, but retain\n"
        "  // the exact affected-work safety guard for low-degree roots.\n"
        "  const bool guarded_root = policy == \"adaptive\" && vertices >= 200000 && root_degree <= 8;\n"
        "  const auto initial_bfs_begin = Clock::now();\n"
        "  const double repair_budget =\n"
        "      policy == \"always_incremental\" ? 2.0 :\n"
        "      ((policy == \"adaptive\" && !guarded_root && vertices >= 200000) ? 2.0 : kAffectedBudget);"
    )

    text = replace_once(
        text,
        "        } else if (shallow_cold_start) {\n          choose_full = true;\n          trace.reason = \"large_shallow_cold_start\";\n        } else if (!have_incremental) {",
        "        } else if (shallow_cold_start) {\n"
        "          choose_full = true;\n"
        "          trace.reason = \"large_shallow_cold_start\";\n"
        "        } else if (guarded_root && initial_reachable_fraction >= kSparseReach &&\n"
        "                   update_fraction >= kLargeGraphUpdateGuard) {\n"
        "          choose_full = true;\n"
        "          trace.reason = \"large_root_locality_update_guard\";\n"
        "        } else if (!have_incremental) {"
    )

    text = replace_once(
        text,
        "          trace.reason = choose_full ? \"large_warmup_full\" : \"large_warmup_incremental\";",
        "          trace.reason = choose_full ? \"large_warmup_full\" :\n"
        "              (guarded_root ? \"large_root_locality_guarded_incremental\" : \"large_warmup_incremental\");"
    )

    text = replace_once(
        text,
        "        if (update_fraction >= kPreflightFullUpdate) {\n          choose_full = true;\n          trace.reason = \"scale_preflight_full\";\n        } else if (initial_reachable_fraction <= kVerySparseReach) {",
        "        if (update_fraction >= kPreflightFullUpdate) {\n"
        "          choose_full = true;\n"
        "          trace.reason = \"scale_preflight_full\";\n"
        "        } else if (root_in_degree >= 256 && root_sink_ratio >= 2.0) {\n"
        "          choose_full = true;\n"
        "          trace.reason = \"scale_root_sink_locality_full\";\n"
        "        } else if (initial_reachable_fraction <= kVerySparseReach) {"
    )

    text = replace_once(
        text,
        "\"schema_version\\\":6,\\\"selector\\\":\\\"scale-conditioned-selector-owned-v3\\\"",
        "\"schema_version\\\":8,\\\"selector\\\":\\\"root-locality-affected-work-v5\\\""
    )

    Path(args.output).write_text(text)


if __name__ == "__main__":
    main()
