#!/usr/bin/env python3
import argparse
import itertools
import json
import os
import platform
import shlex
import subprocess
import sys
import time
from pathlib import Path

SCHEMA_VERSION = 1


def load_config(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("unsupported campaign schema_version")
    if not isinstance(data.get("command"), list) or not data["command"]:
        raise ValueError("command must be a non-empty argv list")
    axes = data.get("axes", {})
    if not isinstance(axes, dict) or any(not isinstance(v, list) or not v for v in axes.values()):
        raise ValueError("axes must map names to non-empty lists")
    repeat = int(data.get("repeat", 1))
    if repeat < 1:
        raise ValueError("repeat must be at least 1")
    return data


def expand_axes(axes):
    if not axes:
        return [{}]
    names = list(axes)
    return [dict(zip(names, values)) for values in itertools.product(*(axes[n] for n in names))]


def render_arg(value, params):
    return str(value).format(**params)


def run_case(command, params, repeat, cwd, env_overrides, timeout):
    argv = [render_arg(arg, params) for arg in command]
    env = os.environ.copy()
    env.update({k: render_arg(v, params) for k, v in env_overrides.items()})
    attempts = []
    for index in range(repeat):
        started = time.perf_counter()
        completed = subprocess.run(
            argv,
            cwd=cwd,
            env=env,
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )
        elapsed = time.perf_counter() - started
        parsed = None
        stdout = completed.stdout.strip()
        if stdout:
            try:
                parsed = json.loads(stdout)
            except json.JSONDecodeError:
                pass
        attempts.append({
            "iteration": index + 1,
            "wall_seconds": elapsed,
            "returncode": completed.returncode,
            "stdout": stdout,
            "stderr": completed.stderr.strip(),
            "json": parsed,
        })
        if completed.returncode != 0:
            break
    return argv, attempts


def main():
    parser = argparse.ArgumentParser(description="Run reproducible VeloGraphX experiment matrices and emit JSON.")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    config = load_config(args.config)
    combinations = expand_axes(config.get("axes", {}))
    repeat = int(config.get("repeat", 1))
    cwd = Path(config.get("cwd", "."))
    env_overrides = config.get("env", {})
    timeout = float(config.get("timeout_seconds", 600))

    report = {
        "schema_version": SCHEMA_VERSION,
        "name": config.get("name", args.config.stem),
        "config": str(args.config),
        "dry_run": args.dry_run,
        "case_count": len(combinations),
        "repeat": repeat,
        "environment": {
            "python": platform.python_version(),
            "platform": platform.platform(),
            "machine": platform.machine(),
            "cpu_count": os.cpu_count(),
        },
        "cases": [],
    }

    for params in combinations:
        argv = [render_arg(arg, params) for arg in config["command"]]
        case = {"parameters": params, "argv": argv, "attempts": []}
        if not args.dry_run:
            argv, attempts = run_case(config["command"], params, repeat, cwd, env_overrides, timeout)
            case["argv"] = argv
            case["attempts"] = attempts
            case["success"] = len(attempts) == repeat and all(a["returncode"] == 0 for a in attempts)
        report["cases"].append(case)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")

    if not args.dry_run and any(not case.get("success", False) for case in report["cases"]):
        return 2
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.TimeoutExpired as exc:
        print(f"error: command timed out: {shlex.join(exc.cmd)}", file=sys.stderr)
        raise SystemExit(3)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
