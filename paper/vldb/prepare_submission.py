#!/usr/bin/env python3
"""Prepare the PVLDB build directory from the canonical Markdown manuscript.

The script keeps `paper/manuscript.md` as the source of scientific prose while
allowing the official PVLDB template to compile in CI. Pandoc performs Markdown
-> LaTeX conversion after this script extracts the abstract/body and normalizes
heading levels for the venue wrapper.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PAPER = ROOT / "paper"
BUILD = PAPER / "vldb" / "build"
MANUSCRIPT = PAPER / "manuscript.md"


def section_between(text: str, start: str, end: str) -> str:
    a = text.index(start) + len(start)
    b = text.index(end, a)
    return text[a:b].strip()


def normalize_body(body: str) -> str:
    out: list[str] = []
    for line in body.splitlines():
        if line.startswith("### "):
            title = re.sub(r"^###\s+\d+(?:\.\d+)*\.?\s*", "## ", line)
            out.append(title)
        elif line.startswith("## "):
            title = re.sub(r"^##\s+\d+(?:\.\d+)*\.?\s*", "# ", line)
            out.append(title)
        else:
            out.append(line)
    return "\n".join(out).strip() + "\n"


def main() -> None:
    text = MANUSCRIPT.read_text(encoding="utf-8")
    BUILD.mkdir(parents=True, exist_ok=True)

    abstract = section_between(text, "## Abstract", "## 1. Introduction")
    body_start = text.index("## 1. Introduction")
    body = normalize_body(text[body_start:])

    (BUILD / "abstract.md").write_text(abstract + "\n", encoding="utf-8")
    (BUILD / "body.md").write_text(body, encoding="utf-8")

    print(f"Prepared abstract/body Markdown under {BUILD}")


if __name__ == "__main__":
    main()
