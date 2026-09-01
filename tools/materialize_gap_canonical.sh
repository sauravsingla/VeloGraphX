#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <gapbs-source-dir> <twitter|web|road|kron|urand> <output-dir>" >&2
  exit 2
fi

GAP_DIR=$(cd "$1" && pwd)
DATASET=$2
OUT=$(mkdir -p "$3" && cd "$3" && pwd)
RAW="$OUT/raw"
mkdir -p "$RAW"

if [[ ! -x "$GAP_DIR/converter" ]]; then
  make -C "$GAP_DIR" -j2 converter
fi

GAP_REV=$(git -C "$GAP_DIR" describe --tags --exact-match 2>/dev/null || true)
if [[ "$GAP_REV" != "v1.5" ]]; then
  echo "canonical materializer requires GAP Benchmark Suite tag v1.5; got '$GAP_REV'" >&2
  exit 3
fi

retry_download() {
  local url=$1
  local path=$2
  for attempt in 1 2 3 4; do
    if curl -fL --retry 3 --retry-delay 5 "$url" -o "$path"; then return 0; fi
    echo "download attempt $attempt failed: $url" >&2
    sleep $((attempt * 5))
  done
  return 1
}

case "$DATASET" in
  twitter)
    parts=()
    for suffix in 00 01 02 03; do
      part="$RAW/twitter_rv.net.$suffix.gz"
      retry_download "https://github.com/ANLAB-KAIST/traces/releases/download/twitter_rv.net/twitter_rv.net.$suffix.gz" "$part"
      parts+=("$part")
    done
    gzip -cd "${parts[@]}" > "$RAW/twitter_rv.net"
    "$GAP_DIR/converter" -f "$RAW/twitter_rv.net" -e "$OUT/twitter.el"
    "$GAP_DIR/converter" -f "$RAW/twitter_rv.net" -e "$OUT/twitter.wel" -w
    ;;
  web)
    archive="$RAW/sk-2005.tar.gz"
    retry_download "https://sparse.tamu.edu/MM/LAW/sk-2005.tar.gz" "$archive"
    tar -xzf "$archive" -C "$RAW"
    matrix="$RAW/sk-2005/sk-2005.mtx"
    test -f "$matrix"
    "$GAP_DIR/converter" -f "$matrix" -e "$OUT/web.el"
    "$GAP_DIR/converter" -f "$matrix" -e "$OUT/web.wel" -w
    ;;
  road)
    archive="$RAW/USA-road-d.USA.gr.gz"
    retry_download "http://www.dis.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.gr.gz" "$archive"
    gzip -cd "$archive" > "$RAW/USA-road-d.USA.gr"
    "$GAP_DIR/converter" -f "$RAW/USA-road-d.USA.gr" -e "$OUT/road.el"
    "$GAP_DIR/converter" -f "$RAW/USA-road-d.USA.gr" -e "$OUT/road.wel" -w
    ;;
  kron)
    "$GAP_DIR/converter" -g27 -k16 -e "$OUT/kron.el"
    "$GAP_DIR/converter" -g27 -k16 -e "$OUT/kron.wel" -w
    ;;
  urand)
    "$GAP_DIR/converter" -u27 -k16 -e "$OUT/urand.el"
    "$GAP_DIR/converter" -u27 -k16 -e "$OUT/urand.wel" -w
    ;;
  *) echo "unknown canonical GAP dataset: $DATASET" >&2; exit 2 ;;
esac

EDGE_SHA=$(sha256sum "$OUT/$DATASET.el" | awk '{print $1}')
WEIGHTED_SHA=$(sha256sum "$OUT/$DATASET.wel" | awk '{print $1}')
printf '%s  %s\n%s  %s\n' "$EDGE_SHA" "$DATASET.el" "$WEIGHTED_SHA" "$DATASET.wel" > "$OUT/$DATASET.sha256"
EDGE_BYTES=$(stat -c%s "$OUT/$DATASET.el")
WEIGHTED_BYTES=$(stat -c%s "$OUT/$DATASET.wel")

case "$DATASET" in
  twitter|web|road) DIRECTED=true ;;
  kron|urand) DIRECTED=false ;;
esac
case "$DATASET" in
  road) DELTA=50000 ;;
  *) DELTA=2 ;;
esac

cat > "$OUT/$DATASET.metadata.json" <<JSON
{
  "schema_version": 2,
  "dataset": "$DATASET",
  "gap_revision": "v1.5",
  "recipe_source": "GAP Benchmark Suite v1.5 benchmark/bench.mk",
  "canonical": true,
  "directed": $DIRECTED,
  "sssp_delta": $DELTA,
  "edge_list": "$DATASET.el",
  "weighted_edge_list": "$DATASET.wel",
  "checksums": {
    "edge_list_sha256": "$EDGE_SHA",
    "weighted_edge_list_sha256": "$WEIGHTED_SHA"
  },
  "sizes_bytes": {
    "edge_list": $EDGE_BYTES,
    "weighted_edge_list": $WEIGHTED_BYTES
  }
}
JSON

sha256sum -c "$OUT/$DATASET.sha256" --ignore-missing
python - "$OUT/$DATASET.metadata.json" "$DATASET" <<'PY'
import json, sys
p, dataset = sys.argv[1:]
m = json.load(open(p))
assert m['schema_version'] == 2
assert m['dataset'] == dataset
assert m['gap_revision'] == 'v1.5'
assert m['canonical'] is True
assert len(m['checksums']['edge_list_sha256']) == 64
assert len(m['checksums']['weighted_edge_list_sha256']) == 64
assert m['sizes_bytes']['edge_list'] > 0
assert m['sizes_bytes']['weighted_edge_list'] > 0
PY

echo "materialized canonical GAP dataset: $DATASET"
cat "$OUT/$DATASET.sha256"
