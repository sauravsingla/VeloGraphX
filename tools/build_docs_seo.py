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
SOCIAL_IMAGE_URL = BASE_URL + "velographx-social-card.png"

CUSTOM = {
    "index.html": (
        "VeloGraphX — Dynamic Graph Analytics & Incremental Algorithms",
        "VeloGraphX documentation for high-performance dynamic graph analytics and incremental graph algorithms in C++20 and Python.",
    ),
    "dynamic_bfs.html": (
        "Dynamic BFS — Exact Breadth-First Search | VeloGraphX",
        "Dynamic BFS in VeloGraphX maintains exact breadth-first-search results on evolving graphs using localized repair or exact recomputation.",
    ),
    "incremental_bfs.html": (
        "Incremental BFS — Exact BFS After Updates | VeloGraphX",
        "Incremental BFS in VeloGraphX maintains exact unweighted shortest-path distances after graph updates with conservative recomputation fallback.",
    ),
    "dynamic_graph_algorithms.html": (
        "Dynamic Graph Algorithms in C++ & Python | VeloGraphX",
        "Explore VeloGraphX dynamic graph algorithms for BFS, SSSP, connected components, triangle counting, k-core, and PageRank workflows.",
    ),
    "incremental_graph_analytics.html": (
        "Incremental Graph Analytics | VeloGraphX",
        "Incremental graph analytics in VeloGraphX combines mutable graph storage, localized maintenance, conservative fallback, and reproducible evaluation.",
    ),
    "dynamic_connected_components.html": (
        "Dynamic Connected Components | VeloGraphX",
        "Dynamic connected components in VeloGraphX maintain exact connectivity as graph updates arrive, with correctness-first evolving-graph semantics.",
    ),
    "dynamic_triangle_counting.html": (
        "Dynamic Triangle Counting | VeloGraphX",
        "Dynamic triangle counting in VeloGraphX maintains exact counts on evolving graphs with reproducible exactness and crossover evidence.",
    ),
    "cpp_graph_analytics_library.html": (
        "C++ Graph Analytics Library + Python | VeloGraphX",
        "VeloGraphX is a C++20 graph analytics library with Python bindings for dynamic graphs, exact maintained analytics, and reproducible benchmarks.",
    ),
}

SEO_START = "<!-- VeloGraphX SEO metadata -->"
SEO_END = "<!-- /VeloGraphX SEO metadata -->"

NOINDEX_EXACT = {
    "annotated.html",
    "classes.html",
    "dirs.html",
    "examples.html",
    "files.html",
    "hierarchy.html",
    "namespaces.html",
    "pages.html",
    "topics.html",
    "search.html",
}


def clean_text(value: str) -> str:
    value = re.sub(r"<[^>]+>", " ", value)
    value = html.unescape(value)
    return " ".join(value.split())


def existing_title(document: str, fallback: str) -> str:
    match = re.search(r"<title>(.*?)</title>", document, flags=re.IGNORECASE | re.DOTALL)
    return clean_text(match.group(1)) if match else fallback


def concise_subject(title: str) -> str:
    subject = re.sub(r"^VeloGraphX:\s*", "", title)
    subject = re.sub(r"\s+(Class Template|Class|Struct|Namespace|File|Concept) Reference$", "", subject)
    subject = re.sub(r"\s+Source File$", " source", subject)
    return subject.strip(" -:")


def description_for(relative: str, title: str) -> str:
    if relative in CUSTOM:
        return CUSTOM[relative][1]

    subject = concise_subject(title)
    if relative.endswith("_source.html"):
        desc = f"C++ source reference for {subject} in the VeloGraphX dynamic graph analytics library."
    elif relative.endswith("-members.html"):
        desc = f"API member index for {subject} in VeloGraphX, a C++20 and Python library for dynamic graph analytics."
    elif "Namespace Reference" in title:
        desc = f"API reference for the {subject} namespace in VeloGraphX dynamic and incremental graph analytics."
    elif "Class" in title or "Struct" in title or "Concept" in title:
        desc = f"API reference for {subject} in VeloGraphX, a C++20 and Python engine for dynamic and incremental graph analytics."
    elif "File Reference" in title:
        desc = f"File-level API reference for {subject} in the VeloGraphX C++20 dynamic graph analytics library."
    else:
        desc = f"VeloGraphX documentation for {subject}, covering C++20 and Python APIs for dynamic and incremental graph analytics."

    trimmed = desc[:157].rstrip(" ,;:-")
    return trimmed if trimmed.endswith(".") else trimmed + "."


def canonical_for(relative_path: str) -> str:
    if relative_path == "index.html":
        return BASE_URL
    return BASE_URL + quote(relative_path, safe="/-_.~")


def is_indexable(relative: str) -> bool:
    name = Path(relative).name
    if relative in CUSTOM:
        return True
    if name in NOINDEX_EXACT:
        return False
    if name.endswith("_source.html") or name.endswith("-members.html") or name.endswith("_8md.html"):
        return False
    if name.startswith("dir_"):
        return False
    if re.match(r"^(functions|globals|namespacemembers)(_.+)?\.html$", name):
        return False
    return True


def metadata_block(title: str, description: str, canonical: str, indexable: bool) -> str:
    title_e = html.escape(title, quote=True)
    desc_e = html.escape(description, quote=True)
    url_e = html.escape(canonical, quote=True)
    site_e = html.escape(SITE_NAME, quote=True)
    image_e = html.escape(SOCIAL_IMAGE_URL, quote=True)
    robots = "index, follow, max-image-preview:large" if indexable else "noindex, follow"

    return f"""{SEO_START}
<meta name=\"description\" content=\"{desc_e}\" />
<meta name=\"robots\" content=\"{robots}\" />
<link rel=\"canonical\" href=\"{url_e}\" />
<meta property=\"og:type\" content=\"website\" />
<meta property=\"og:site_name\" content=\"{site_e}\" />
<meta property=\"og:title\" content=\"{title_e}\" />
<meta property=\"og:description\" content=\"{desc_e}\" />
<meta property=\"og:url\" content=\"{url_e}\" />
<meta property=\"og:image\" content=\"{image_e}\" />
<meta property=\"og:image:alt\" content=\"VeloGraphX — dynamic graph analytics in C++20 and Python\" />
<meta name=\"twitter:card\" content=\"summary_large_image\" />
<meta name=\"twitter:title\" content=\"{title_e}\" />
<meta name=\"twitter:description\" content=\"{desc_e}\" />
<meta name=\"twitter:image\" content=\"{image_e}\" />
{SEO_END}"""


def process_page(root: Path, path: Path) -> tuple[str, bool, str]:
    relative = path.relative_to(root).as_posix()
    document = path.read_text(encoding="utf-8")
    original_title = existing_title(document, SITE_NAME)
    title = CUSTOM.get(relative, (original_title, ""))[0]
    description = description_for(relative, title)
    canonical = canonical_for(relative)
    indexable = is_indexable(relative)

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

    block = metadata_block(title, description, canonical, indexable)
    if "</head>" not in document.lower():
        raise RuntimeError(f"Missing </head> in {path}")
    document = re.sub(r"</head>", block + "\n</head>", document, count=1, flags=re.IGNORECASE)
    path.write_text(document, encoding="utf-8")
    return canonical, indexable, description


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


def validate_output(root: Path, page_records: list[tuple[str, bool, str]]) -> None:
    required = list(CUSTOM)
    missing = [name for name in required if not (root / name).is_file()]
    if missing:
        raise SystemExit("expected SEO landing pages were not generated: " + ", ".join(missing))

    if not (root / "velographx-social-card.png").is_file():
        raise SystemExit("missing velographx-social-card.png in generated documentation")

    required_fragments = (
        '<meta name="description"',
        '<link rel="canonical"',
        '<meta property="og:title"',
        '<meta property="og:description"',
        '<meta property="og:url"',
        '<meta property="og:image"',
        '<meta name="twitter:card"',
        '<meta name="twitter:image"',
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

    indexable_descriptions = [description for _url, indexable, description in page_records if indexable]
    duplicates = sorted({d for d in indexable_descriptions if indexable_descriptions.count(d) > 1})
    if duplicates:
        raise SystemExit("duplicate meta descriptions remain on indexable pages")

    sitemap_path = root / "sitemap.xml"
    robots_path = root / "robots.txt"
    if not sitemap_path.is_file() or not robots_path.is_file():
        raise SystemExit("sitemap.xml or robots.txt was not generated")

    sitemap = html.unescape(sitemap_path.read_text(encoding="utf-8"))
    for relative in CUSTOM:
        canonical = canonical_for(relative)
        if canonical not in sitemap:
            raise SystemExit(f"landing page missing from sitemap: {canonical}")

    excluded_urls = [url for url, indexable, _description in page_records if not indexable]
    leaked = [url for url in excluded_urls if url in sitemap]
    if leaked:
        raise SystemExit(f"noindex pages leaked into sitemap: {leaked[:3]}")

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

    records = [process_page(root, page) for page in pages]
    indexable_urls = [url for url, indexable, _description in records if indexable]
    write_sitemap(root, indexable_urls)
    write_robots(root)
    validate_output(root, records)

    print(
        f"SEO metadata validated on {len(pages)} HTML pages; "
        f"{len(indexable_urls)} indexable URLs emitted to sitemap"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
