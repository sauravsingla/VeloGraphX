#!/usr/bin/env python3
"""Apply the Issue #61 bounded one-sided warm-up selector candidate.

When a large-graph selector has a measured full-recompute cost but no incremental
observation yet, retain the measured full baseline for one one-sided warm-up
batch, then permit an incremental probe. This fixes the reported second-batch
cold-start decision without allowing the selector to remain permanently stuck
on full recomputation.
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
        '  bool first_batch = true;\n',
        '  bool first_batch = true;\n'
        '  bool one_sided_full_warmup_used = false;\n'
    )
    text = replace_once(
        text,
        '        } else if (!have_incremental) {\n'
        '          choose_full = update_fraction >= simple_update_fraction;\n'
        '          trace.reason = choose_full ? "large_warmup_full" : "large_warmup_incremental";\n',
        '        } else if (!have_incremental && have_full && !one_sided_full_warmup_used) {\n'
        '          // Full cost is measured while incremental cost is unknown. Keep the measured\n'
        '          // baseline for one warm-up decision, then allow an incremental calibration probe.\n'
        '          choose_full = true;\n'
        '          one_sided_full_warmup_used = true;\n'
        '          trace.reason = "large_one_sided_full";\n'
        '        } else if (!have_incremental) {\n'
        '          choose_full = update_fraction >= simple_update_fraction;\n'
        '          trace.reason = choose_full ? "large_warmup_full" : "large_one_sided_probe_incremental";\n'
    )
    text = replace_once(
        text,
        '\"schema_version\\\":6,\\\"selector\\\":\\\"scale-conditioned-selector-owned-v3\\\"',
        '\"schema_version\\\":7,\\\"selector\\\":\\\"bounded-one-sided-warmup-v5\\\"'
    )
    Path(args.output).write_text(text)


if __name__ == "__main__":
    main()
