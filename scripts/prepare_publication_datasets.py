#!/usr/bin/env python3
"""Prepare publication benchmark datasets with provenance metadata.

Examples:
  python3 scripts/prepare_publication_datasets.py roadnet-ca --output-dir data
  python3 scripts/prepare_publication_datasets.py kronecker --output-dir data --scale 20 --seed 1
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import random
import urllib.request
from pathlib import Path

ROADNET_CA_URL = "https://snap.stanford.edu/data/roadNet-CA.txt.gz"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def prepare_roadnet_ca(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / "roadNet-CA.txt.gz"
    normalized = output_dir / "roadNet-CA.edges"
    metadata = output_dir / "roadNet-CA.metadata.json"

    if not archive.exists():
        urllib.request.urlretrieve(ROADNET_CA_URL, archive)

    vertices: set[int] = set()
    source_rows = 0
    with gzip.open(archive, "rt", encoding="utf-8") as src, normalized.open("w", encoding="utf-8") as dst:
        for line in src:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            u_text, v_text = stripped.split()[:2]
            u = int(u_text)
            v = int(v_text)
            if u == v:
                continue
            dst.write(f"{u} {v}\n")
            vertices.add(u)
            vertices.add(v)
            source_rows += 1

    # SNAP's roadNet-CA text file contains both orientations of each undirected
    # road. The public dataset page reports 2,766,607 undirected edges, while
    # the source text therefore contains 5,533,214 non-comment edge rows.
    expected_vertices = 1965206
    expected_undirected_edges = 2766607
    expected_source_rows = expected_undirected_edges * 2
    record = {
        "dataset": "roadNet-CA",
        "source_url": ROADNET_CA_URL,
        "source_sha256": sha256_file(archive),
        "normalized_sha256": sha256_file(normalized),
        "vertices_observed": len(vertices),
        "source_edge_rows_observed": source_rows,
        "expected_source_vertices": expected_vertices,
        "expected_source_edge_rows": expected_source_rows,
        "expected_undirected_edges": expected_undirected_edges,
        "normalization": "comments removed; self-loops removed; both source orientations retained; VeloGraphX undirected loader canonicalizes duplicate reciprocal input",
    }
    metadata.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    if record["vertices_observed"] != expected_vertices or record["source_edge_rows_observed"] != expected_source_rows:
        raise RuntimeError("roadNet-CA provenance counts do not match the publication contract")
    print(json.dumps(record, indent=2))


def generate_kronecker(output_dir: Path, scale: int, edgefactor: int, seed: int) -> None:
    if scale <= 0 or scale >= 32:
        raise ValueError("scale must be in [1, 31] for VeloGraphX uint32 vertex IDs")
    if edgefactor <= 0:
        raise ValueError("edgefactor must be positive")

    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / f"kron-scale{scale}-ef{edgefactor}-seed{seed}.edges"
    metadata = path.with_suffix(".metadata.json")
    rng = random.Random(seed)
    n = 1 << scale
    m = edgefactor * n
    probabilities = (0.57, 0.19, 0.19, 0.05)

    with path.open("w", encoding="utf-8") as handle:
        for _ in range(m):
            u = 0
            v = 0
            step = n >> 1
            while step:
                r = rng.random()
                if r < probabilities[0]:
                    pass
                elif r < probabilities[0] + probabilities[1]:
                    v += step
                elif r < probabilities[0] + probabilities[1] + probabilities[2]:
                    u += step
                else:
                    u += step
                    v += step
                step >>= 1
            handle.write(f"{u} {v}\n")

    record = {
        "dataset": "kronecker",
        "generator": "Graph500-style R-MAT initiator; deterministic Python reference generator",
        "scale": scale,
        "vertices": n,
        "edgefactor": edgefactor,
        "generated_edge_tuples": m,
        "A": probabilities[0],
        "B": probabilities[1],
        "C": probabilities[2],
        "D": probabilities[3],
        "seed": seed,
        "sha256": sha256_file(path),
        "notes": "Raw tuple stream intentionally retains self-loops and duplicate edges; graph loaders may canonicalize according to their documented semantics.",
    }
    metadata.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(record, indent=2))


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    road = subparsers.add_parser("roadnet-ca")
    road.add_argument("--output-dir", type=Path, required=True)

    kron = subparsers.add_parser("kronecker")
    kron.add_argument("--output-dir", type=Path, required=True)
    kron.add_argument("--scale", type=int, required=True)
    kron.add_argument("--edgefactor", type=int, default=16)
    kron.add_argument("--seed", type=int, default=1)

    args = parser.parse_args()
    if args.command == "roadnet-ca":
        prepare_roadnet_ca(args.output_dir)
    else:
        generate_kronecker(args.output_dir, args.scale, args.edgefactor, args.seed)


if __name__ == "__main__":
    main()
