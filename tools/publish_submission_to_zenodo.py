#!/usr/bin/env python3
"""Publish a frozen VeloGraphX submission archive to Zenodo.

The script is intentionally opt-in through an access token. It first searches
the authenticated deposition workspace for an existing record with the exact
submission title/version so workflow retries do not create duplicate drafts.
If an unpublished matching draft exists it is reused; if a matching published
record exists its DOI is returned without creating another record.
"""
from __future__ import annotations

import argparse
import json
import os
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

BASE = "https://zenodo.org/api"


def request(method: str, url: str, token: str, body=None, content_type="application/json"):
    data = None
    headers = {"Authorization": f"Bearer {token}"}
    if body is not None:
        if isinstance(body, (dict, list)):
            data = json.dumps(body).encode()
        elif isinstance(body, bytes):
            data = body
        else:
            raise TypeError(type(body))
        headers["Content-Type"] = content_type
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=180) as response:
            raw = response.read()
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")
        raise RuntimeError(f"Zenodo HTTP {exc.code} for {url}: {detail}") from exc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bundle", type=Path, required=True)
    ap.add_argument("--metadata", type=Path, default=Path(".zenodo.json"))
    ap.add_argument("--freeze", type=Path, default=Path("paper/submission-freeze.json"))
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--token-env", default="ZENODO_ACCESS_TOKEN")
    args = ap.parse_args()

    token = os.environ.get(args.token_env, "").strip()
    if not token:
        raise SystemExit(f"{args.token_env} is not configured")
    if not args.bundle.is_file():
        raise SystemExit(f"bundle not found: {args.bundle}")

    metadata = json.loads(args.metadata.read_text())
    freeze = json.loads(args.freeze.read_text())
    title = metadata["title"]
    version = freeze["tag"]

    query = urllib.parse.quote(f'title:"{title}"')
    candidates = request("GET", f"{BASE}/deposit/depositions?q={query}&size=100", token)
    matching = []
    for dep in candidates:
        md = dep.get("metadata") or {}
        if md.get("title") == title and md.get("version") == version:
            matching.append(dep)

    published = next((d for d in matching if d.get("submitted")), None)
    if published:
        result = {
            "status": "already_published",
            "deposition_id": published.get("id"),
            "doi": published.get("doi") or (published.get("metadata") or {}).get("doi"),
            "record_url": (published.get("links") or {}).get("record_html"),
            "version": version,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        print(json.dumps(result, sort_keys=True))
        return

    deposition = matching[0] if matching else request("POST", f"{BASE}/deposit/depositions", token, {})
    dep_id = deposition["id"]
    links = deposition.get("links") or {}
    bucket = links.get("bucket")
    if not bucket:
        # Refresh an existing draft to recover its bucket link.
        deposition = request("GET", f"{BASE}/deposit/depositions/{dep_id}", token)
        bucket = (deposition.get("links") or {}).get("bucket")
    if not bucket:
        raise RuntimeError("Zenodo draft did not provide an upload bucket")

    with args.bundle.open("rb") as handle:
        request(
            "PUT",
            f"{bucket}/{urllib.parse.quote(args.bundle.name)}",
            token,
            handle.read(),
            content_type="application/gzip",
        )

    creators = metadata.get("creators", [{"name": "Singla, Saurav"}])
    deposit_metadata = {
        "metadata": {
            "title": title,
            "upload_type": metadata.get("upload_type", "software"),
            "description": metadata["description"],
            "creators": creators,
            "license": metadata.get("license", "Apache-2.0"),
            "access_right": metadata.get("access_right", "open"),
            "keywords": metadata.get("keywords", []),
            "version": version,
            "related_identifiers": [
                {
                    "identifier": "https://github.com/sauravsingla/VeloGraphX",
                    "relation": "isSupplementTo",
                    "resource_type": "software"
                }
            ],
        }
    }
    request("PUT", f"{BASE}/deposit/depositions/{dep_id}", token, deposit_metadata)
    published = request("POST", f"{BASE}/deposit/depositions/{dep_id}/actions/publish", token, {})
    result = {
        "status": "published",
        "deposition_id": published.get("id", dep_id),
        "doi": published.get("doi") or (published.get("metadata") or {}).get("doi"),
        "record_url": (published.get("links") or {}).get("record_html"),
        "version": version,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
