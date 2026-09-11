#!/usr/bin/env python3
"""Apply the Issue #61 one-sided warm-up selector candidate.

When a large-graph selector has a measured full-recompute cost but no incremental
observation yet, prefer that measured baseline instead of forcing an unmeasured
incremental warm-up. This directly addresses the one-sided-model failure reported
in #61. Incremental exploration remains available in regimes where the selector
has not yet obtained a full-cost observation.
"""
from pathlib import Path
import argparse


def replace_once(text: str, old: str, new: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one match, found {count}: {old!r}")
    return text.replace(old, new, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()
    text = Path(args.input).read_text()

    text = replace_once(
        text,
        '        } else if (!have_incremental) {\n'
        '          choose_full = update_fraction >= simple_update_fraction;\n'
        '          trace.reason = choose_full ? "large_warmup_full" : "large_warmup_incremental";\n',
        '        } else if (!have_incremental && have_full) {\n'
        '          // One-sided model: full cost is measured while incremental cost is unknown.\n'
        '          // Do not force an unmeasured incremental warm-up; retain the measured baseline.\n'
        '          choose_full = true;\n'
        '          trace.reason = "large_one_sided_full";\n'
        '        } else if (!have_incremental) {\n'
        '          choose_full = update_fraction >= simple_update_fraction;\n'
        '          trace.reason = choose_full ? "large_warmup_full" : "large_warmup_incremental";\n'
    )
    text = replace_once(
        text,
        '\"schema_version\\\":6,\\\"selector\\\":\\\"scale-conditioned-selector-owned-v3\\\"',
        '\"schema_version\\\":7,\\\"selector\\\":\\\"one-sided-warmup-v4\\\"'
    )
    Path(args.output).write_text(text)


if __name__ == "__main__":
    main()
