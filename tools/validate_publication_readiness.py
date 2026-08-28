#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate the VeloGraphX publication-hardware campaign contract.")
    parser.add_argument("plan", type=Path)
    parser.add_argument("--expect-ready", action="store_true", help="Require the publication claim gate to be open.")
    args = parser.parse_args()

    data = json.loads(args.plan.read_text(encoding="utf-8"))
    require(data.get("schema_version") == 1, "unsupported schema_version")
    require(data.get("artifact_type") == "velographx-publication-hardware-plan", "unexpected artifact_type")

    hardware = data["hardware"]
    require(hardware["dedicated_machine_required"] is True, "dedicated hardware must be required")
    require(hardware["linux_required"] is True, "Linux must be required")
    metadata = set(hardware["required_metadata"])
    for field in {"cpu_model", "sockets", "physical_cores", "logical_cpus", "numa_nodes", "memory_bytes", "compiler", "compiler_version", "kernel", "build_flags", "affinity_policy"}:
        require(field in metadata, f"missing hardware metadata field: {field}")

    experiments = data["experiments"]
    threads = experiments["thread_scaling"]["threads"]
    require(threads[:6] == [1, 2, 4, 8, 16, 32], "thread scaling must cover 1/2/4/8/16/32")
    require(experiments["thread_scaling"]["minimum_repeats"] >= 10, "thread scaling needs at least 10 repeats")
    require(experiments["numa"]["requires_at_least_two_numa_nodes"] is True, "NUMA claims require >=2 NUMA nodes")
    require("cross-node" in experiments["numa"]["policies"], "NUMA plan must include cross-node placement")
    require(experiments["hardware_counters"]["required"] is True, "hardware counters must be required")
    require(experiments["ablations"]["required"] is True, "ablations must be required")
    require(experiments["competitors"]["same_hardware_required"] is True, "competitors must run on same hardware")
    require("LAGraph" in " ".join(experiments["competitors"]["priority_native"]), "LAGraph/GraphBLAS must be a priority competitor")
    require("GAP Benchmark Suite" in experiments["competitors"]["priority_native"], "GAP must be a priority competitor")

    datasets = data["datasets"]
    require(datasets["immutable_provenance_required"] is True, "dataset provenance must be immutable")
    require(datasets["checksums_required"] is True, "dataset checksums must be required")
    require(datasets["minimum_distinct_graph_families"] >= 3, "at least three graph families are required")

    stats = data["statistics"]
    require(stats["raw_samples_required"] is True, "raw samples must be retained")
    require(stats["median_required"] is True and stats["p95_required"] is True, "median and p95 must be reported")
    require(stats["uncertainty_summary_required"] is True, "uncertainty summary must be required")

    artifacts = data["artifacts"]
    require(artifacts["environment_capture_required"] is True, "environment capture is required")
    require(artifacts["validated_result_bundle_required"] is True, "validated result bundle is required")

    gate = data["claim_gate"]
    require(gate["hosted_ci_alone_is_publication_grade"] is False, "hosted CI must not be publication grade")
    require(gate["failed_correctness_blocks_performance_claims"] is True, "correctness failure must block claims")
    if args.expect_ready:
        require(data.get("publication_ready") is True, "publication_ready is false")
        require(gate["allow_publication_claims"] is True, "publication claim gate is closed")
    else:
        require(data.get("publication_ready") is False, "contract fixture must remain not-ready until real hardware evidence exists")
        require(gate["allow_publication_claims"] is False, "publication claims must remain disabled in the repository fixture")

    print(json.dumps({
        "schema_version": 1,
        "artifact_type": "velographx-publication-readiness-validation",
        "valid": True,
        "publication_ready": data["publication_ready"],
        "allow_publication_claims": gate["allow_publication_claims"]
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
