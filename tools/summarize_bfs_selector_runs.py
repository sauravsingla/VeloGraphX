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


def summarize(directory: Path) -> dict:
    regrets: list[float] = []
    decision_us: list[float] = []
    wrong = 0
    total = 0
    full_choices = 0
    fallbacks = 0
    all_exact = True
    regimes: list[dict] = []

    for path in sorted(directory.glob('*.json')):
        d = json.loads(path.read_text())
        all_exact = all_exact and bool(d.get('all_policies_exact', False))
        policies = {p['name']: p for p in d['policies']}
        adaptive = policies['adaptive']
        inc = policies['always_incremental']['batch_us']
        full = policies['always_full']['batch_us']
        selected = adaptive['batch_us']
        chosen_full = adaptive.get('explicit_full', [False] * len(selected))
        internal = adaptive.get('internal_full_fallback', [False] * len(selected))
        decision_us.extend(adaptive.get('decision_us', []))

        local: list[float] = []
        local_wrong = 0
        for i, value in enumerate(selected):
            oracle_full = not (inc[i] < full[i])
            oracle = full[i] if oracle_full else inc[i]
            regret = max(0.0, (value - oracle) / oracle) if oracle > 0 else 0.0
            local.append(regret)
            regrets.append(regret)
            w = bool(chosen_full[i]) != oracle_full
            local_wrong += int(w)
            wrong += int(w)
            total += 1
            full_choices += int(bool(chosen_full[i]))
            fallbacks += int(bool(internal[i]))

        regimes.append({
            'file': path.name,
            'samples': len(selected),
            'mean_regret': statistics.mean(local) if local else 0.0,
            'p95_regret': percentile(local, 0.95),
            'max_regret': max(local, default=0.0),
            'wrong_arm_rate': local_wrong / len(selected) if selected else 0.0,
        })

    return {
        'all_exact': all_exact,
        'files': len(regimes),
        'samples': total,
        'mean_regret': statistics.mean(regrets) if regrets else 0.0,
        'median_regret': percentile(regrets, 0.50),
        'p95_regret': percentile(regrets, 0.95),
        'p99_regret': percentile(regrets, 0.99),
        'max_regret': max(regrets, default=0.0),
        'wrong_arm_rate': wrong / total if total else 0.0,
        'full_choice_fraction': full_choices / total if total else 0.0,
        'internal_fallback_fraction': fallbacks / total if total else 0.0,
        'mean_decision_us': statistics.mean(decision_us) if decision_us else 0.0,
        'regimes': regimes,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    result = summarize(args.input)
    result.update({
        'schema_version': 1,
        'artifact_type': 'velographx-bfs-selector-summary',
        'research_claim': False,
    })
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result['all_exact'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
