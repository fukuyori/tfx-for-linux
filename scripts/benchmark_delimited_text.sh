#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
build_root="${TFX_BENCHMARK_BUILD_ROOT:-${TMPDIR:-/tmp}/tfx-delimited-benchmark}"
rows="${TFX_BENCHMARK_ROWS:-1000}"
columns="${TFX_BENCHMARK_COLUMNS:-100}"
iterations="${TFX_BENCHMARK_ITERATIONS:-20}"

for value in "$rows" "$columns" "$iterations"; do
    if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
        echo "Benchmark rows, columns, and iterations must be positive integers." >&2
        exit 2
    fi
done

run_benchmark() {
    local label="$1"
    local rust_core="$2"
    local build_dir="$build_root/$label"

    cmake -S "$repo_root/linux-qt" -B "$build_dir" \
        -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DTFX_BUILD_BENCHMARKS=ON \
        -DTFX_ENABLE_RUST_CORE="$rust_core"
    cmake --build "$build_dir" --target tfx_delimited_text_benchmark --parallel
    "$build_dir/tfx_delimited_text_benchmark" "$rows" "$columns" "$iterations"
}

run_benchmark cpp OFF
run_benchmark rust ON
