#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-release}"
OUT_DIR="${OUT_DIR:-perf-results}"
TARGET="${1:-velographx_dynamic_benchmark}"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "perf profiling is Linux-only; no measurements collected on $(uname -s)."
  exit 0
fi

if ! command -v perf >/dev/null 2>&1; then
  echo "perf is not installed; no measurements collected."
  exit 0
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DVELOGRAPHX_BUILD_TESTS=OFF -DVELOGRAPHX_BUILD_BENCHMARKS=ON
cmake --build "$BUILD_DIR" -j "${JOBS:-2}"
mkdir -p "$OUT_DIR"

BIN="$BUILD_DIR/$TARGET"
if [[ ! -x "$BIN" ]]; then
  echo "benchmark target not found: $BIN" >&2
  exit 2
fi

TS="$(date -u +%Y%m%dT%H%M%SZ)"
CSV="$OUT_DIR/${TARGET}-${TS}.perf.csv"
META="$OUT_DIR/${TARGET}-${TS}.meta.json"

EVENTS="cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses"

set +e
perf stat -x, -o "$CSV" -e "$EVENTS" -- "$BIN"
STATUS=$?
set -e

python3 - "$META" "$TARGET" "$STATUS" <<'PY'
import json, os, platform, subprocess, sys
path, target, status = sys.argv[1], sys.argv[2], int(sys.argv[3])
def cmd(args):
    try:
        return subprocess.check_output(args, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return None
meta = {
    "target": target,
    "perf_exit_status": status,
    "platform": platform.platform(),
    "machine": platform.machine(),
    "processor": platform.processor(),
    "kernel": platform.release(),
    "cpu_count": os.cpu_count(),
    "git_commit": cmd(["git", "rev-parse", "HEAD"]),
    "compiler": cmd(["c++", "--version"]),
    "note": "Hardware-counter availability depends on kernel permissions and CPU support; unavailable counters must not be treated as benchmark failures."
}
with open(path, "w", encoding="utf-8") as f:
    json.dump(meta, f, indent=2)
PY

if [[ $STATUS -ne 0 ]]; then
  echo "perf could not collect one or more counters (status $STATUS). This is an environment limitation, not a benchmark failure."
  exit 0
fi

echo "Wrote $CSV"
echo "Wrote $META"
