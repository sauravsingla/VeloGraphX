#!/usr/bin/env python3
"""Add search/social metadata to generated Doxygen HTML and emit crawl files."""

from __future__ import annotations

import html
import re
import sys
from pathlib import Path
from urllib.parse import quote

BASE_URL = "https://sauravsingla.github.io/VeloGraphX/"
SITE_NAME = "VeloGraphX"
DEFAULT_DESCRIPTION = (
    "VeloGraphX is a high-performance C++20 and Python engine for dynamic and "
    "incremental graph analytics, including BFS, SSSP, connected components, "
    "triangle counting, k-core, and PageRank workflows."
)

CUSTOM = {
    "index.html": (
        "VeloGraphX — Dynamic Graph Analytics & Incremental Graph Algorithms",
        "VeloGraphX documentation for high-performance dynamic graph analytics and incremental graph algorithms in C++20 and Python.",
    ),
    "dynamic_bfs.html": (
        "Dynamic BFS — Exact Dynamic Breadth-First Search | VeloGraphX",
        "Dynamic BFS in VeloGraphX: exact breadth-first-search maintenance on evolving graphs with localized repair, full recomputation, and adaptive plan selection.",
    ),
    "incremental_bfs.html": (
        "Incremental BFS — Maintain Exact BFS After Graph Updates | VeloGraphX",
        "Incremental BFS in VeloGraphX maintains exact unweighted shortest-path distances after graph updates and can fall back to exact recomputation when appropriate.",
    ),
    "dynamic_graph_algorithms.html": (
        "Dynamic Graph Algorithms in C++20 and Python | VeloGraphX",
        "Explore VeloGraphX dynamic graph algorithms for BFS, SSSP, connected components, triangle counting, k-core, and PageRank-related workflows.",
    ),
    "incremental_graph_analytics.html": (
        "Incremental Graph Analytics for Evolving Graphs | VeloGraphX",
        "Incremental graph analytics in VeloGraphX combines mutable graph storage, localized maintenance, conservative fallback, and reproducible evaluation.",
    ),
    "dynamic_connected_components.html": (
        "Dynamic Connected Components — Exact Connectivity | VeloGraphX",
        "Dynamic connected components in VeloGraphX maintain exact connectivity as graph updates arrive, with correctness-first handling of evolving graph state.",
    ),
    "dynamic_triangle_counting.html": (
        "Dynamic Triangle Counting — Exact Evolving-Graph Counts | VeloGraphX",
        "Dynamic triangle counting in VeloGraphX maintains exact triangle counts on evolving graphs and includes reproducible exactness and crossover evidence.",
    ),
    "cpp_graph_analytics_library.html": (
        "C++ Graph Analytics Library with Python Bindings | VeloGraphX",
        "VeloGraphX is a C++20 graph analytics library with Python bindings for dynamic graphs, exact maintained analytics, multicore execution, and reproducible benchmarking.",
    ),
}

SEO_START = "<!-- VeloGraphX SEO metadata -->"
SEO_END = "<!-- /VeloGraphX SEO metadata -->"


def clean_text(value: str) -> str:
    value = re.sub(r"<[^>]+>", " ", value)
    value = html.unescape(value)
    return " ".join(value.split())


def existing_title(document: str, fallback: str) -> str:
    match = re.search(r"<title>(.*?)</title>", document, flags=re.IGNORECASE | re.DOTALL)
    return clean_text(match.group(1)) if match else fallback


def canonical_for(relative_path: str) -> str:
    if relative_path == "index.html":
        return BASE_URL
    return BASE_URL + quote(relative_path, safe="/-_.~")


def metadata_block(title: str, description: str, canonical: str) -> str:
    title_e = html.escape(title, quote=True)
    desc_e = html.escape(description, quote=True)
    url_e = html.escape(canonical, quote=True)
    site_e = html.escape(SITE_NAME, quote=True)
    return f"""{SEO_START}
<meta name=\"description\" content=\"{desc_e}\" />
<meta name=\"robots\" content=\"index, follow, max-image-preview:large\" />
<link rel=\"canonical\" href=\"{url_e}\" />
<meta property=\"og:type\" content=\"website\" />
<meta property=\"og:site_name\" content=\"{site_e}\" />
<meta property=\"og:title\" content=\"{title_e}\" />
<meta property=\"og:description\" content=\"{desc_e}\" />
<meta property=\"og:url\" content=\"{url_e}\" />
<meta name=\"twitter:card\" content=\"summary\" />
<meta name=\"twitter:title\" content=\"{title_e}\" />
<meta name=\"twitter:description\" content=\"{desc_e}\" />
{SEO_END}"""


def process_page(root: Path, path: Path) -> str:
    relative = path.relative_to(root).as_posix()
    document = path.read_text(encoding="utf-8")
    title, description = CUSTOM.get(
        relative,
        (existing_title(document, SITE_NAME), DEFAULT_DESCRIPTION),
    )
    canonical = canonical_for(relative)

    # Keep generated output idempotent if the postprocessor is run more than once.
    document = re.sub(
        rf"\s*{re.escape(SEO_START)}.*?{re.escape(SEO_END)}\s*",
        "\n",
        document,
        flags=re.DOTALL,
    )

    if relative in CUSTOM:
        escaped_title = html.escape(title)
        if re.search(r"<title>.*?</title>", document, flags=re.IGNORECASE | re.DOTALL):
            document = re.sub(
                r"<title>.*?</title>",
                f"<title>{escaped_title}</title>",
                document,
                count=1,
                flags=re.IGNORECASE | re.DOTALL,
            )

    block = metadata_block(title, description, canonical)
    if "</head>" not in document.lower():
        raise RuntimeError(f"Missing </head> in {path}")
    document = re.sub(r"</head>", block + "\n</head>", document, count=1, flags=re.IGNORECASE)
    path.write_text(document, encoding="utf-8")
    return canonical


def write_sitemap(root: Path, urls: list[str]) -> None:
    entries = "\n".join(f"  <url><loc>{html.escape(url)}</loc></url>" for url in sorted(set(urls)))
    sitemap = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        f"{entries}\n"
        "</urlset>\n"
    )
    (root / "sitemap.xml").write_text(sitemap, encoding="utf-8")


def write_robots(root: Path) -> None:
    robots = "User-agent: *\nAllow: /\n\nSitemap: " + BASE_URL + "sitemap.xml\n"
    (root / "robots.txt").write_text(robots, encoding="utf-8")


def validate_output(root: Path) -> None:
    required = list(CUSTOM)
    missing = [name for name in required if not (root / name).is_file()]
    if missing:
        raise SystemExit("expected SEO landing pages were not generated: " + ", ".join(missing))

    required_fragments = (
        '<meta name="description"',
        '<link rel="canonical"',
        '<meta property="og:title"',
        '<meta property="og:description"',
        '<meta property="og:url"',
        '<meta name="twitter:card"',
    )

    for relative, (title, _description) in CUSTOM.items():
        document = (root / relative).read_text(encoding="utf-8")
        absent = [fragment for fragment in required_fragments if fragment not in document]
        if absent:
            raise SystemExit(f"missing SEO metadata in {relative}: {', '.join(absent)}")
        if title not in html.unescape(document):
            raise SystemExit(f"expected page title not found in {relative}: {title}")
        canonical = canonical_for(relative)
        if canonical not in html.unescape(document):
            raise SystemExit(f"expected canonical URL not found in {relative}: {canonical}")

    sitemap_path = root / "sitemap.xml"
    robots_path = root / "robots.txt"
    if not sitemap_path.is_file() or not robots_path.is_file():
        raise SystemExit("sitemap.xml or robots.txt was not generated")

    sitemap = html.unescape(sitemap_path.read_text(encoding="utf-8"))
    for relative in CUSTOM:
        canonical = canonical_for(relative)
        if canonical not in sitemap:
            raise SystemExit(f"landing page missing from sitemap: {canonical}")

    robots = robots_path.read_text(encoding="utf-8")
    expected_sitemap = "Sitemap: " + BASE_URL + "sitemap.xml"
    if expected_sitemap not in robots:
        raise SystemExit("robots.txt does not advertise the sitemap")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: build_docs_seo.py <doxygen-html-dir>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit(f"not a directory: {root}")

    pages = sorted(root.rglob("*.html"))
    if not pages:
        raise SystemExit(f"no HTML pages found under {root}")

    urls = [process_page(root, page) for page in pages]
    write_sitemap(root, urls)
    write_robots(root)
    validate_output(root)

    print(f"SEO metadata validated on {len(pages)} HTML pages; sitemap and robots.txt generated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
