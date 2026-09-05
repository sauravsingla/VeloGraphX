#!/usr/bin/env python3
"""Generate a conservative SPDX 2.3 JSON SBOM for VeloGraphX source intake."""

from __future__ import annotations

import argparse
import json
import subprocess
from datetime import datetime, timezone
from importlib import metadata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT_VERSION = "0.8.0"

OPTIONAL_PYTHON = ["pybind11", "numpy", "scipy", "pyarrow"]


def git_revision() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
    except Exception:
        return "NOASSERTION"


def installed_version(name: str) -> str:
    try:
        return metadata.version(name)
    except metadata.PackageNotFoundError:
        return "NOASSERTION"


def package(spdx_id: str, name: str, version: str, *, comment: str, purl_name: str | None = None) -> dict:
    item = {
        "SPDXID": spdx_id,
        "name": name,
        "versionInfo": version,
        "downloadLocation": "NOASSERTION",
        "filesAnalyzed": False,
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": "NOASSERTION",
        "copyrightText": "NOASSERTION",
        "comment": comment,
    }
    if purl_name and version != "NOASSERTION":
        item["externalRefs"] = [{
            "referenceCategory": "PACKAGE-MANAGER",
            "referenceType": "purl",
            "referenceLocator": f"pkg:pypi/{purl_name}@{version}",
        }]
    return item


def build_document() -> dict:
    revision = git_revision()
    namespace_rev = revision if revision != "NOASSERTION" else "unknown"
    packages = [
        {
            "SPDXID": "SPDXRef-Package-VeloGraphX",
            "name": "VeloGraphX",
            "versionInfo": PROJECT_VERSION,
            "downloadLocation": "https://github.com/sauravsingla/VeloGraphX",
            "filesAnalyzed": False,
            "licenseConcluded": "Apache-2.0",
            "licenseDeclared": "Apache-2.0",
            "copyrightText": "NOASSERTION",
            "comment": f"Source revision: {revision}. Core default CMake build declares no mandatory third-party runtime dependency.",
        },
        package(
            "SPDXRef-Package-liburing",
            "liburing",
            "NOASSERTION",
            comment="Optional Linux dependency used only when VELOGRAPHX_ENABLE_IO_URING=ON.",
        ),
    ]

    relationships = [{
        "spdxElementId": "SPDXRef-DOCUMENT",
        "relationshipType": "DESCRIBES",
        "relatedSpdxElement": "SPDXRef-Package-VeloGraphX",
    }, {
        "spdxElementId": "SPDXRef-Package-VeloGraphX",
        "relationshipType": "OPTIONAL_DEPENDENCY_OF",
        "relatedSpdxElement": "SPDXRef-Package-liburing",
        "comment": "liburing is optional and disabled by default.",
    }]

    for name in OPTIONAL_PYTHON:
        spdx_id = f"SPDXRef-Package-python-{name.lower()}"
        version = installed_version(name)
        packages.append(package(
            spdx_id,
            name,
            version,
            comment="Optional Python build/interoperability dependency; Python bindings are disabled by default.",
            purl_name=name,
        ))
        relationships.append({
            "spdxElementId": spdx_id,
            "relationshipType": "OPTIONAL_DEPENDENCY_OF",
            "relatedSpdxElement": "SPDXRef-Package-VeloGraphX",
        })

    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": "VeloGraphX-source-intake-sbom",
        "documentNamespace": f"https://github.com/sauravsingla/VeloGraphX/sbom/{namespace_rev}",
        "creationInfo": {
            "created": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
            "creators": ["Tool: VeloGraphX tools/generate_spdx_sbom.py"],
        },
        "packages": packages,
        "relationships": relationships,
        "documentComment": "Source-intake SBOM. Generate an environment-specific SBOM after resolving platform-specific and optional dependencies for production use.",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="build/velographx-sbom.spdx.json")
    args = parser.parse_args()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(build_document(), indent=2) + "\n", encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
