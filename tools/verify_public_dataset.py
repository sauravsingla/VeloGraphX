#!/usr/bin/env python3
import argparse
import hashlib
import json
import tempfile
import urllib.request
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def inspect_edge_list(path: Path, directed: bool):
    raw_edges = 0
    self_loops = 0
    vertices = set()
    normalized = set()
    malformed = 0

    with path.open("r", encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                malformed += 1
                continue
            try:
                u, v = int(parts[0]), int(parts[1])
            except ValueError:
                malformed += 1
                continue
            if u < 0 or v < 0:
                raise ValueError(f"negative vertex id at line {lineno}")
            raw_edges += 1
            vertices.update((u, v))
            if u == v:
                self_loops += 1
                continue
            edge = (u, v) if directed or u < v else (v, u)
            normalized.add(edge)

    return {
        "raw_edge_rows": raw_edges,
        "unique_vertices": len(vertices),
        "self_loop_rows": self_loops,
        "normalized_edge_count": len(normalized),
        "duplicate_or_reverse_rows": raw_edges - self_loops - len(normalized),
        "malformed_rows": malformed,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Download and inspect a public edge-list dataset without making benchmark claims.")
    parser.add_argument("--name", required=True)
    parser.add_argument("--url", required=True)
    parser.add_argument("--canonical-source", required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--directed", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmpdir:
        target = Path(tmpdir) / "dataset.txt"
        req = urllib.request.Request(args.url, headers={"User-Agent": "VeloGraphX-dataset-verifier/1"})
        with urllib.request.urlopen(req, timeout=60) as response, target.open("wb") as out:
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                out.write(chunk)

        report = {
            "schema_version": 1,
            "artifact_type": "velographx-public-dataset-verification",
            "research_claim": False,
            "name": args.name,
            "canonical_source": args.canonical_source,
            "download_url": args.url,
            "source_revision": args.source_revision,
            "sha256": sha256_file(target),
            "bytes": target.stat().st_size,
            "format": "edge-list-text",
            "directed": args.directed,
            "inspection": inspect_edge_list(target, args.directed),
        }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
