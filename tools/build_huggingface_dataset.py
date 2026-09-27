#!/usr/bin/env python3
"""Build the Hugging Face benchmark/reproducibility bundle from versioned VeloGraphX artifacts.

This exporter deliberately copies only repository-authored benchmark, provenance,
methodology, and reproduction material. It does not download or redistribute
third-party graph datasets and it does not generate benchmark measurements.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "hf_bundle"


def copy_file(src: str, dst: str | None = None) -> None:
    source = ROOT / src
    if not source.is_file():
        raise FileNotFoundError(source)
    target = OUT / (dst or src)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def copy_tree(src: str, dst: str) -> None:
    source = ROOT / src
    if not source.is_dir():
        raise FileNotFoundError(source)
    target = OUT / dst
    shutil.copytree(source, target, dirs_exist_ok=True)


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True, stderr=subprocess.STDOUT
    ).strip()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    # Hugging Face dataset card and top-level archival metadata.
    copy_file("huggingface/README.md", "README.md")
    for path in ("CITATION.cff", "LICENSE", "PAPER.md", "REPRODUCIBILITY.md"):
        copy_file(path)

    # Viewer-friendly canonical data copied without modification.
    copy_file("paper/data/current-selector-regimes.csv", "data/current-selector-regimes.csv")
    copy_file("paper/data/accepted-results.json", "data/accepted-results.json")
    copy_file("paper/data/final-review-derived.json", "data/final-review-derived.json")

    # Full repository-authored benchmark and data-contract trees.
    copy_tree("benchmarks", "artifacts/benchmarks")
    copy_tree("datasets", "artifacts/datasets")
    copy_tree("paper/data", "artifacts/paper-data")

    # Paper-facing ledgers and frozen submission provenance.
    for path in (
        "paper/results-ledger.md",
        "paper/submission-closure-evidence.json",
        "paper/submission-freeze.json",
        "paper/validate_submission_data.py",
        "paper/README.md",
    ):
        copy_file(path, f"artifacts/paper/{Path(path).name}")

    # Methodology, evidence boundaries, limitations, and archival status.
    methodology_docs = (
        "docs/benchmark-methodology.md",
        "docs/paper-evidence-index.md",
        "docs/paper-claims.md",
        "docs/controlled-hardware-execution.md",
        "docs/publication-hardware-methodology.md",
        "docs/limitations.md",
        "docs/submission-archive.md",
        "docs/ablation-study.md",
        "docs/hosted-native-competitors.md",
        "docs/external-dynamic-baselines.md",
        "docs/canonical-publication-campaign.md",
        "docs/same-run-published-baseline.md",
        "docs/canonicalization-ab-evidence.md",
        "docs/networkit-fair-benchmark-contract.md",
        "docs/graphbolt-dzig-gap-benchmark-contract.md",
    )
    for path in methodology_docs:
        copy_file(path, f"methodology/{Path(path).name}")

    # Retained research-result and preregistration documents relevant to the
    # adaptive selector. Negative results are included intentionally.
    research_docs = (
        "docs/research/bfs-selector-v3-h6-preregistration.md",
        "docs/research/bfs-selector-v3-h6-results.md",
        "docs/research/selector-v2-generalization-preregistration.md",
        "docs/research/selector-v2-generalization-results.md",
        "docs/research/post-pr71-reviewer-closure.md",
        "docs/research/post-pr71-reviewer-closure-results.md",
        "docs/research/repair-recompute-policy-evaluation.md",
        "docs/research/selector-tail-regret-incident.md",
    )
    for path in research_docs:
        copy_file(path, f"methodology/research/{Path(path).name}")

    # Minimal reproduction and public-dataset preparation scripts.
    reproduction_scripts = (
        "scripts/reproduce_minimal.sh",
        "scripts/prepare_publication_datasets.py",
        "scripts/run_canonical_dataset_campaign.py",
        "scripts/run_capacity_campaign.py",
    )
    for path in reproduction_scripts:
        copy_file(path, f"reproduction/scripts/{Path(path).name}")

    # Environment capture, result construction, summarization and validation.
    reproduction_tools = (
        "tools/build_result_artifact.py",
        "tools/capture_benchmark_environment.py",
        "tools/summarize_adaptive_policy.py",
        "tools/summarize_bfs_selector_runs.py",
        "tools/summarize_publication_policy.py",
        "tools/summarize_selector_ablation.py",
        "tools/summarize_selector_v2.py",
        "tools/summarize_update_crossover.py",
        "tools/validate_benchmark_preflight.py",
        "tools/validate_campaign_readiness.py",
        "tools/validate_public_dataset_plan.py",
        "tools/validate_publication_measurements.py",
        "tools/validate_publication_readiness.py",
        "tools/validate_result_bundle.py",
        "tools/verify_public_dataset.py",
    )
    for path in reproduction_tools:
        copy_file(path, f"reproduction/tools/{Path(path).name}")

    source_commit = os.environ.get("GITHUB_SHA") or git("rev-parse", "HEAD")
    try:
        source_commit_date = git("show", "-s", "--format=%cI", source_commit)
    except subprocess.CalledProcessError:
        source_commit_date = None

    files = []
    for path in sorted(p for p in OUT.rglob("*") if p.is_file()):
        rel = path.relative_to(OUT).as_posix()
        files.append(
            {
                "path": rel,
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
        )

    manifest = {
        "schema_version": 1,
        "artifact": "VeloGraphX Hugging Face benchmark and reproducibility bundle",
        "source_repository": "https://github.com/sauravsingla/VeloGraphX",
        "source_commit": source_commit,
        "source_commit_date": source_commit_date,
        "huggingface_repository": "datasets/sauravsingla08/velographx-benchmarks",
        "measurement_policy": "No benchmark values are generated by this exporter; versioned evidence is copied from the source repository.",
        "third_party_raw_graphs_redistributed": False,
        "files": files,
    }
    manifest_path = OUT / "_provenance" / "source_manifest.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"Built {len(files) + 1} files in {OUT}")
    print(f"Source commit: {source_commit}")


if __name__ == "__main__":
    main()
