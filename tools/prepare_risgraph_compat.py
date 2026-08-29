#!/usr/bin/env python3
"""Apply reproducible build-only compatibility fixes to pinned RisGraph.

This script intentionally does not modify RisGraph algorithm implementations.
It adapts legacy TBB discovery/API usage for modern oneTBB and adds a missing
standard-library include to RisGraph's pinned Abseil dependency for modern GCC.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def run(*args: str, cwd: Path | None = None) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--risgraph", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    repo = args.risgraph.resolve()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)

    risgraph_head = run("git", "rev-parse", "HEAD", cwd=repo)
    abseil = repo / "deps" / "abseil-cpp"
    abseil_head = run("git", "rev-parse", "HEAD", cwd=abseil)

    finder = repo / "cmake" / "FindTBB.cmake"
    (out / "FindTBB.upstream.cmake").write_bytes(finder.read_bytes())
    finder.write_text(
        "# VeloGraphX benchmark compatibility shim for RisGraph legacy TBB discovery.\n"
        "# Dependency discovery only; no RisGraph graph/algorithm source is changed.\n"
        "find_path(TBB_INCLUDE_DIRS NAMES tbb/parallel_sort.h)\n"
        "find_library(TBB_LIBRARY NAMES tbb)\n"
        "include(FindPackageHandleStandardArgs)\n"
        "find_package_handle_standard_args(TBB REQUIRED_VARS TBB_INCLUDE_DIRS TBB_LIBRARY)\n"
        "set(TBB_LIBRARIES \"${TBB_LIBRARY}\")\n"
        "get_filename_component(TBB_LIBRARY_DIRS \"${TBB_LIBRARY}\" DIRECTORY)\n"
        "mark_as_advanced(TBB_INCLUDE_DIRS TBB_LIBRARY TBB_LIBRARY_DIRS)\n"
    )
    (out / "FindTBB.compat.cmake").write_bytes(finder.read_bytes())

    graph = repo / "core" / "graph.hpp"
    graph_text = graph.read_text()
    old_scheduler = "tbb::task_scheduler_init init(ncpus);"
    new_scheduler = (
        "tbb::global_control init(tbb::global_control::max_allowed_parallelism, "
        "static_cast<std::size_t>(ncpus));"
    )
    if old_scheduler in graph_text:
        graph_text = graph_text.replace(old_scheduler, new_scheduler, 1)
        graph.write_text(graph_text)
    elif new_scheduler not in graph_text:
        raise RuntimeError("cannot locate pinned RisGraph task_scheduler_init call")

    graphcycles = abseil / "absl" / "synchronization" / "internal" / "graphcycles.cc"
    text = graphcycles.read_text()
    if "#include <limits>" not in text:
        marker = "#include <algorithm>\n"
        if marker not in text:
            raise RuntimeError("cannot locate stable include insertion point in pinned Abseil")
        text = text.replace(marker, marker + "#include <limits>\n", 1)
        graphcycles.write_text(text)

    risgraph_patch = run(
        "git", "diff", "--", "cmake/FindTBB.cmake", "core/graph.hpp", cwd=repo
    ) + "\n"
    abseil_patch = run(
        "git", "diff", "--", "absl/synchronization/internal/graphcycles.cc", cwd=abseil
    ) + "\n"
    if not risgraph_patch.strip() or not abseil_patch.strip():
        raise RuntimeError("expected compatibility patches were not produced")

    risgraph_patch_path = out / "risgraph-build-compat.patch"
    abseil_patch_path = out / "abseil-gcc-compat.patch"
    risgraph_patch_path.write_text(risgraph_patch)
    abseil_patch_path.write_text(abseil_patch)

    changed_parent = run("git", "diff", "--name-only", cwd=repo).splitlines()
    allowed_parent = {"cmake/FindTBB.cmake", "core/graph.hpp", "deps/abseil-cpp"}
    if set(changed_parent) - allowed_parent:
        raise RuntimeError(f"unexpected RisGraph changes: {changed_parent}")
    changed_abseil = run("git", "diff", "--name-only", cwd=abseil).splitlines()
    if changed_abseil != ["absl/synchronization/internal/graphcycles.cc"]:
        raise RuntimeError(f"unexpected Abseil changes: {changed_abseil}")

    manifest = {
        "schema_version": 1,
        "artifact_type": "risgraph-build-compatibility-manifest",
        "risgraph_revision": risgraph_head,
        "abseil_revision": abseil_head,
        "algorithm_implementation_modified": False,
        "compatibility_source_modified": True,
        "changes": [
            {
                "scope": "RisGraph build system",
                "path": "cmake/FindTBB.cmake",
                "reason": "legacy finder requires tbb_stddef.h removed from modern oneTBB",
            },
            {
                "scope": "RisGraph runtime initialization compatibility",
                "path": "core/graph.hpp",
                "reason": "replace removed tbb::task_scheduler_init with oneTBB global_control while preserving requested parallelism during initialization",
            },
            {
                "scope": "pinned Abseil dependency",
                "path": "absl/synchronization/internal/graphcycles.cc",
                "reason": "add explicit <limits> include required by modern GCC",
                "patch_sha256": sha256(abseil_patch_path),
            },
        ],
        "risgraph_compat_patch_sha256": sha256(risgraph_patch_path),
        "research_claim": False,
    }
    (out / "compatibility-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
