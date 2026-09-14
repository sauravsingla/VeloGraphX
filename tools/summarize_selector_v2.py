#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path


def percentile(values: list[float], q: float) -> float:
    if not values:
        return 0.0
    data = sorted(values)
    if len(data) == 1:
        return data[0]
    pos = (len(data) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return data[lo]
    w = pos - lo
    return data[lo] * (1.0 - w) + data[hi] * w


def load(path: Path) -> list[tuple[Path, dict]]:
    return [(p, json.loads(p.read_text())) for p in sorted(path.glob('*.json'))]


def summarize(items: list[tuple[Path, dict]]) -> dict:
    regrets: list[float] = []
    decisions: list[float] = []
    wrong = 0
    total = 0
    full_choices = 0
    fallbacks = 0
    all_exact = True
    regimes = []

    for path, d in items:
        all_exact = all_exact and bool(d.get('all_policies_exact', False))
        policies = {p['name']: p for p in d['policies']}
        target = policies['adaptive']
        inc = policies['always_incremental']['batch_us']
        full = policies['always_full']['batch_us']
        selected = target['batch_us']
        explicit_full = target.get('explicit_full', [False] * len(selected))
        internal = target.get('internal_full_fallback', [False] * len(selected))
        decisions.extend(target.get('decision_us', []))

        local_regrets: list[float] = []
        local_wrong = 0
        for i, value in enumerate(selected):
            oracle_full = not (inc[i] < full[i])
            oracle = full[i] if oracle_full else inc[i]
            regret = max(0.0, (value - oracle) / oracle) if oracle > 0 else 0.0
            local_regrets.append(regret)
            regrets.append(regret)
            is_wrong = bool(explicit_full[i]) != oracle_full
            wrong += int(is_wrong)
            local_wrong += int(is_wrong)
            total += 1
            full_choices += int(bool(explicit_full[i]))
            fallbacks += int(bool(internal[i]))

        regimes.append({
            'file': path.name,
            'samples': len(selected),
            'mean_regret': statistics.mean(local_regrets) if local_regrets else 0.0,
            'p95_regret': percentile(local_regrets, 0.95),
            'max_regret': max(local_regrets, default=0.0),
            'wrong_arm_rate': local_wrong / len(selected) if selected else 0.0,
        })

    return {
        'all_exact': all_exact,
        'files': len(items),
        'samples': total,
        'mean_regret': statistics.mean(regrets) if regrets else 0.0,
        'median_regret': percentile(regrets, 0.50),
        'p95_regret': percentile(regrets, 0.95),
        'p99_regret': percentile(regrets, 0.99),
        'max_regret': max(regrets, default=0.0),
        'wrong_arm_rate': wrong / total if total else 0.0,
        'full_choice_fraction': full_choices / total if total else 0.0,
        'internal_fallback_fraction': fallbacks / total if total else 0.0,
        'mean_decision_us': statistics.mean(decisions) if decisions else 0.0,
        'regimes': regimes,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--development-bfs', type=Path, required=True)
    ap.add_argument('--development-triangles', type=Path, required=True)
    ap.add_argument('--holdout-bfs', type=Path, required=True)
    ap.add_argument('--holdout-triangles', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()

    summary = {
        'schema_version': 1,
        'artifact_type': 'velographx-selector-v2-generalization-summary',
        'research_claim': False,
        'development_bfs': summarize(load(args.development_bfs)),
        'development_triangles': summarize(load(args.development_triangles)),
        'holdout_bfs': summarize(load(args.holdout_bfs)),
        'holdout_triangles': summarize(load(args.holdout_triangles)),
    }
    summary['all_exact'] = all(
        summary[k]['all_exact'] for k in (
            'development_bfs', 'development_triangles', 'holdout_bfs', 'holdout_triangles'
        )
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if summary['all_exact'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
