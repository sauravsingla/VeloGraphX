#!/usr/bin/env python3
"""Apply the Issue #61 one-sided warm-up selector candidate.

The candidate keeps exploration for very small update batches, but when a large-graph
selector has a measured full-recompute cost and no incremental observation yet, it
uses a stricter bounded-probe threshold (25% of the configured simple threshold)
instead of treating the entire simple-threshold region as safe exploration.
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
        'constexpr std::size_t kFreshAge = 4;\n',
        'constexpr std::size_t kFreshAge = 4;\n'
        'constexpr double kOneSidedWarmupProbeRatio = 0.25;\n'
    )
    text = replace_once(
        text,
        '        } else if (!have_incremental) {\n'
        '          choose_full = update_fraction >= simple_update_fraction;\n'
        '          trace.reason = choose_full ? "large_warmup_full" : "large_warmup_incremental";\n',
        '        } else if (!have_incremental && have_full) {\n'
        '          // One-sided model: full cost is known but incremental cost is not.\n'
        '          // Preserve exploration only for a deliberately small probe region;\n'
        '          // otherwise prefer the measured baseline until an incremental sample exists.\n'
        '          const double probe_fraction =\n'
        '              simple_update_fraction * kOneSidedWarmupProbeRatio;\n'
        '          choose_full = update_fraction >= probe_fraction;\n'
        '          trace.reason = choose_full ? "large_one_sided_full" : "large_one_sided_probe_incremental";\n'
        '        } else if (!have_incremental) {\n'
        '          choose_full = update_fraction >= simple_update_fraction;\n'
        '          trace.reason = choose_full ? "large_warmup_full" : "large_warmup_incremental";\n'
    )
    text = replace_once(
        text,
        '\"schema_version\\\":6,\\\"selector\\\":\\\"scale-conditioned-selector-owned-v3\\\"',
        '\"schema_version\\\":7,\\\"selector\\\":\\\"one-sided-warmup-v4\\\"'
    )
    text = replace_once(
        text,
        '            << ",\\\"fresh_age\\\":" << kFreshAge << "}"\n',
        '            << ",\\\"fresh_age\\\":" << kFreshAge\n'
        '            << ",\\\"one_sided_warmup_probe_ratio\\\":" << kOneSidedWarmupProbeRatio << "}"\n'
    )
    Path(args.output).write_text(text)


if __name__ == "__main__":
    main()
