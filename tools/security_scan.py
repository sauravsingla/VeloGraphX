#!/usr/bin/env python3
"""Fail CI on high-confidence secret/key signatures in tracked text files.

This is a lightweight repository guard, not a replacement for enterprise secret scanning.
"""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAX_FILE_BYTES = 2_000_000

PATTERNS = {
    "private-key": re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----"),
    "aws-access-key": re.compile(rb"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b"),
    "github-token": re.compile(rb"\bgh[pousr]_[A-Za-z0-9_]{30,255}\b"),
    "github-fine-grained-token": re.compile(rb"\bgithub_pat_[A-Za-z0-9_]{20,255}\b"),
    "slack-token": re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{10,}\b"),
}

ALLOWLIST_PATHS = {
    "tools/security_scan.py",
}


def tracked_files() -> list[str]:
    output = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT)
    return [item.decode("utf-8") for item in output.split(b"\0") if item]


def main() -> int:
    findings: list[tuple[str, str]] = []
    for rel in tracked_files():
        if rel in ALLOWLIST_PATHS:
            continue
        path = ROOT / rel
        if not path.is_file() or path.stat().st_size > MAX_FILE_BYTES:
            continue
        try:
            data = path.read_bytes()
        except OSError:
            continue
        if b"\0" in data:
            continue
        for name, pattern in PATTERNS.items():
            if pattern.search(data):
                findings.append((rel, name))

    if findings:
        print("Potential high-confidence secrets detected:")
        for rel, name in findings:
            print(f"  - {rel}: {name}")
        print("Remove the secret from git history as appropriate and rotate it before continuing.")
        return 1

    print(f"Security scan passed: {len(tracked_files())} tracked files checked for high-confidence secret signatures.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
