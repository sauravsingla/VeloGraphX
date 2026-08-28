#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from pathlib import Path


def run_report(repo: Path, dataset: Path, source: int, directed: bool, repeat: int,
               framework: str, *, external_name=None, external_command=None):
    cmd = [
        sys.executable,
        str(repo / "tools" / "competitor_benchmark.py"),
        "--dataset", str(dataset),
        "--framework", framework,
        "--source", str(source),
        "--repeat", str(repeat),
    ]
    if directed:
        cmd.append("--directed")
    if framework == "external":
        cmd.extend([
            "--external-name", external_name,
            "--external-command", external_command,
        ])
    proc = subprocess.run(cmd, text=True, capture_output=True, check=False, cwd=repo)
    if proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip()
        raise RuntimeError(f"{external_name or framework} failed: {detail}")
    return json.loads(proc.stdout)


def require_equal(label, reports, key):
    values = {name: report[key] for name, report in reports.items()}
    if len(set(values.values())) != 1:
        raise RuntimeError(f"normalized BFS mismatch for {label}: {values}")
    return next(iter(values.values()))


def main():
    p = argparse.ArgumentParser(description="Validate normalized BFS semantics across builtin, LAGraph and GAP native adapters.")
    p.add_argument("--dataset", type=Path, required=True)
    p.add_argument("--source", type=int, default=0)
    p.add_argument("--directed", action="store_true")
    p.add_argument("--repeat", type=int, default=3)
    p.add_argument("--lagraph-command", required=True)
    p.add_argument("--gap-command", required=True)
    p.add_argument("--output", type=Path)
    args = p.parse_args()

    if args.repeat < 1:
        raise ValueError("--repeat must be at least 1")
    if not args.dataset.is_file():
        raise ValueError(f"dataset does not exist: {args.dataset}")

    repo = Path(__file__).resolve().parents[1]
    reports = {
        "builtin": run_report(repo, args.dataset, args.source, args.directed, args.repeat, "builtin"),
        "lagraph": run_report(
            repo, args.dataset, args.source, args.directed, args.repeat, "external",
            external_name="SuiteSparse:GraphBLAS/LAGraph",
            external_command=args.lagraph_command,
        ),
        "gap": run_report(
            repo, args.dataset, args.source, args.directed, args.repeat, "external",
            external_name="GAP Benchmark Suite",
            external_command=args.gap_command,
        ),
    }

    normalized = {
        "dataset_sha256": require_equal("dataset checksum", reports, "dataset_sha256"),
        "source": require_equal("source", reports, "source"),
        "directed": require_equal("directedness", reports, "directed"),
        "vertices": require_equal("vertex count", reports, "vertices"),
        "edges": require_equal("edge count", reports, "edges"),
        "result_digest": require_equal("full BFS distance digest", reports, "result_digest"),
        "reachable_vertices": require_equal("reachable vertex count", reports, "reachable_vertices"),
    }

    payload = {
        "schema_version": 1,
        "artifact_type": "velographx-normalized-native-bfs",
        "algorithm": "bfs",
        "normalized_cross_engine_claim": True,
        "correctness_gate": {
            "passed": True,
            "same_dataset": True,
            "same_source": True,
            "same_directedness": True,
            "same_full_distance_digest": True,
        },
        "normalized": normalized,
        "reports": reports,
        "research_claim": False,
        "publication_grade": False,
        "claim_gate": {
            "publication_ready": False,
            "allowed_claim": "Normalized same-dataset/source/directedness BFS correctness and hosted engineering timing only; no publication-grade superiority claim.",
        },
    }

    text = json.dumps(payload, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
