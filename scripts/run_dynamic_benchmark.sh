#!/usr/bin/env sh
set -eu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVELOGRAPHX_BUILD_BENCHMARKS=ON
cmake --build build -j
./build/velographx_dynamic_benchmark
