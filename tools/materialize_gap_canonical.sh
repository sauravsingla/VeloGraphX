#!/usr/bin/env bash
set -euo pipefail

# Materialise one official GAP Benchmark Suite v1.5 corpus graph as plain edge
# lists so VeloGraphX and GAP consume exactly the same bytes. The graph recipes
# mirror benchmark/bench.mk; do not reduce -g27/-u27 and call the result canonical.

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

retry_download() {
  local url=$1
  local path=$2
  for attempt in 1 2 3 4; do
    if curl -fL --retry 3 --retry-delay 5 "$url" -o "$path"; then
      return 0
    fi
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
    # Exact GAP v1.5 benchmark/bench.mk: KRON_ARGS = -g27 -k16
    "$GAP_DIR/converter" -g27 -k16 -e "$OUT/kron.el"
    "$GAP_DIR/converter" -g27 -k16 -e "$OUT/kron.wel" -w
    ;;
  urand)
    # Exact GAP v1.5 benchmark/bench.mk: URAND_ARGS = -u27 -k16
    "$GAP_DIR/converter" -u27 -k16 -e "$OUT/urand.el"
    "$GAP_DIR/converter" -u27 -k16 -e "$OUT/urand.wel" -w
    ;;
  *)
    echo "unknown canonical GAP dataset: $DATASET" >&2
    exit 2
    ;;
esac

sha256sum "$OUT/$DATASET.el" "$OUT/$DATASET.wel" > "$OUT/$DATASET.sha256"
cat > "$OUT/$DATASET.metadata.json" <<JSON
{
  "schema_version": 1,
  "dataset": "$DATASET",
  "gap_revision": "v1.5",
  "recipe_source": "GAP benchmark/bench.mk",
  "canonical": true,
  "edge_list": "$DATASET.el",
  "weighted_edge_list": "$DATASET.wel"
}
JSON

echo "materialized canonical GAP dataset: $DATASET"
cat "$OUT/$DATASET.sha256"
