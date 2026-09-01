#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import socket
import time
import urllib.error
import urllib.request


def git_blob_sha1(data: bytes) -> str:
    payload = f"blob {len(data)}\0".encode("ascii") + data
    return hashlib.sha1(payload).hexdigest()


def download_with_retry(url: str, *, attempts: int = 5, timeout: int = 30) -> bytes:
    """Download immutable public data with bounded retries for transient network failures."""
    last_error: Exception | None = None
    headers = {"User-Agent": "VeloGraphX-pinned-dataset-fetch/1"}

    for attempt in range(1, attempts + 1):
        try:
            request = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(request, timeout=timeout) as response:
                return response.read()
        except (urllib.error.URLError, ConnectionError, TimeoutError, socket.timeout) as exc:
            last_error = exc
            if attempt == attempts:
                break
            delay = min(2 ** (attempt - 1), 8)
            print(
                f"dataset download attempt {attempt}/{attempts} failed: {exc}; "
                f"retrying in {delay}s",
                flush=True,
            )
            time.sleep(delay)

    raise RuntimeError(
        f"dataset download failed after {attempts} attempts: {last_error}"
    ) from last_error


def main() -> int:
    parser = argparse.ArgumentParser(description="Acquire one immutable public dataset and verify its Git blob identity.")
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported manifest schema_version")
    entries = {entry["name"]: entry for entry in manifest.get("datasets", [])}
    if args.dataset not in entries:
        raise ValueError(f"unknown dataset: {args.dataset}")
    entry = entries[args.dataset]

    if entry.get("research_claim") is not False or manifest.get("research_claim") is not False:
        raise ValueError("hosted-CI public datasets must remain research_claim=false")
    revision = entry.get("source_revision", "")
    expected_blob = entry.get("git_blob_sha1", "").lower()
    url = entry.get("url", "")
    if len(revision) != 40 or any(c not in "0123456789abcdef" for c in revision.lower()):
        raise ValueError("source_revision must be a full 40-character Git commit SHA")
    if len(expected_blob) != 40 or any(c not in "0123456789abcdef" for c in expected_blob):
        raise ValueError("git_blob_sha1 must be a 40-character lowercase hex digest")
    if revision not in url:
        raise ValueError("dataset URL must contain the immutable source_revision")

    data = download_with_retry(url)
    actual_blob = git_blob_sha1(data)
    if actual_blob != expected_blob:
        raise ValueError(f"Git blob mismatch: expected {expected_blob}, got {actual_blob}")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    target = args.output_dir / f"{args.dataset}.txt"
    target.write_bytes(data)
    sha256 = hashlib.sha256(data).hexdigest()

    report = {
        "schema_version": 1,
        "artifact_type": "velographx-pinned-public-dataset",
        "research_claim": False,
        "prepared": [{
            "name": entry["name"],
            "family": entry["family"],
            "role": entry["role"],
            "path": str(target),
            "canonical_source": entry["canonical_source"],
            "source_revision": revision,
            "git_blob_sha1": actual_blob,
            "sha256": sha256,
            "format": entry.get("format"),
            "directed": entry.get("directed"),
            "weighted": entry.get("weighted"),
        }],
    }
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
