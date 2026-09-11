#!/usr/bin/env python3
import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "datasets/networkit-fair-benchmark-plan.json")
    data = json.loads(path.read_text())

    require(data.get("issue") == 63, "plan must target Issue #63")
    guidance = data["guidance"]
    require(guidance.get("networkit_endorsement") is False, "guidance must not be described as NetworKit endorsement")

    nk = data["networkit"]
    require(nk["language"] == "C++", "NetworKit comparison must be native C++")
    require("Traversal::BFSfrom" in nk["static_bfs_api"], "missing Traversal::BFSfrom")
    require("Traversal::DijkstraFrom" in nk["static_dijkstra_api"], "missing Traversal::DijkstraFrom")
    require("DynBFS" in nk["dynamic_bfs_api"], "missing DynBFS")
    require("GraphBuilder" in nk["bulk_builder"], "missing bulk graph builder")

    timing = data["timing_contract"]
    require(timing["algorithm_only"] and timing["graph_build_inclusive"], "both timing boundaries are required")
    require(timing["thread_affinity_required"], "thread affinity must be recorded")
    require(timing["correctness_gate_required"], "correctness must be a hard gate")
    flags = set(timing["required_compile_flags"])
    require({"-O3", "-DNDEBUG", "-std=c++20", "-fopenmp"}.issubset(flags), "matched release flags are incomplete")

    rows = data["expanded_campaign"]
    families = {r["family"] for r in rows}
    kinds = {r["kind"] for r in rows}
    names = {r["name"] for r in rows}
    for required in {"com-LiveJournal", "GAP-twitter", "sk-2005", "kron-27", "urand-27"}:
        require(required in names, f"missing recommended large dataset: {required}")
    require("real-world" in kinds and "generated" in kinds, "campaign must mix real-world and generated graphs")
    require(any("infrastructure" in f or "road" in f for f in families), "campaign must include infrastructure/road-like graphs")
    require(any(r.get("cache_scale_target") for r in rows), "campaign needs explicit cache-scale targets")

    policy = data["claim_policy"]
    require(policy["do_not_call_guidance_an_endorsement"], "claim policy must prohibit endorsement language")
    require(policy["do_not_publish_unrun_large_dataset_results"], "claim policy must prohibit unrun-result claims")
    print("Issue #63 NetworKit fair-benchmark plan: PASS")


if __name__ == "__main__":
    main()
