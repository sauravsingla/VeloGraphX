#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${VELOGRAPHX_BUILD_DIR:-"$repo_root/build-paper-minimal"}
artifact_dir=${VELOGRAPHX_ARTIFACT_DIR:-"$repo_root/artifacts/paper-minimal"}
dataset="$artifact_dir/synthetic-evolving-graph.txt"
result="$artifact_dir/adaptive_policy.json"

command -v cmake >/dev/null 2>&1 || {
  echo "error: cmake is required" >&2
  exit 1
}
command -v python3 >/dev/null 2>&1 || {
  echo "error: python3 is required" >&2
  exit 1
}

mkdir -p "$artifact_dir"

echo "[1/5] Configuring focused paper-artifact build"
cmake -S "$repo_root" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVELOGRAPHX_BUILD_TESTS=ON \
  -DVELOGRAPHX_BUILD_BENCHMARKS=ON

echo "[2/5] Building exactness tests and adaptive-policy benchmark"
cmake --build "$build_dir" --parallel --target \
  velographx_test_incremental \
  velographx_test_incremental_bfs_deletion \
  velographx_test_randomized_dynamic \
  velographx_test_execution_plan \
  velographx_adaptive_policy_bfs

echo "[3/5] Running focused correctness tests"
ctest --test-dir "$build_dir" --output-on-failure \
  -R '^(incremental|incremental_bfs_deletion|randomized_dynamic|execution_plan)$'

echo "[4/5] Generating deterministic evolving-graph workload"
python3 - "$dataset" <<'PY'
from pathlib import Path
import sys

out = Path(sys.argv[1])
vertices = 4000
edges = []

# Initial connected structure: 9,000 directed edges.
for u in range(vertices):
    edges.append((u, (u + 1) % vertices))
for u in range(vertices):
    edges.append((u, (u + 17) % vertices))
for u in range(1000):
    edges.append((u, (u + 31) % vertices))

# Later update stream: 3,000 new edges. With imported_rate=0.75 the
# adaptive benchmark imports exactly the 9,000-edge initial structure.
for u in range(3000):
    edges.append((u, (u + 101) % vertices))

assert len(edges) == 12000
out.write_text("".join(f"{u} {v}\n" for u, v in edges), encoding="utf-8")
print(f"wrote {len(edges)} edges to {out}")
PY

echo "[5/5] Running adaptive vs. fixed execution policies"
"$build_dir/velographx_adaptive_policy_bfs" \
  "$dataset" 0 0.75 100 0.01 > "$result"

python3 - "$result" <<'PY'
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))

if not data.get("all_policies_exact", False):
    raise SystemExit("error: at least one policy disagreed with the exact BFS reference")

print("all_policies_exact=true")
for policy in data.get("policies", []):
    print(
        f"{policy['name']}: "
        f"total_us={policy['total_us']:.3f}, "
        f"mean_batch_us={policy['mean_batch_us']:.3f}, "
        f"full_recompute_batches={policy['full_recompute_batches']}"
    )
print(f"machine-readable result: {path}")
print("note: timings from this quick validation are sanity evidence, not publication-grade measurements")
PY
