#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
build_root="${TFX_BENCHMARK_BUILD_ROOT:-${TMPDIR:-/tmp}/tfx-type-ahead-benchmark}"
name_count="${TFX_BENCHMARK_NAMES:-10000}"
iterations="${TFX_BENCHMARK_ITERATIONS:-200}"

if [[ ! "$name_count" =~ ^[1-9][0-9]*$ || ! "$iterations" =~ ^[1-9][0-9]*$ ]]; then
    echo "TFX_BENCHMARK_NAMES and TFX_BENCHMARK_ITERATIONS must be positive integers." >&2
    exit 2
fi

run_benchmark() {
    local label="$1"
    local rust_core="$2"
    local build_dir="$build_root/$label"

    cmake -S "$repo_root/linux-qt" -B "$build_dir" \
        -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DTFX_BUILD_BENCHMARKS=ON \
        -DTFX_ENABLE_RUST_CORE="$rust_core"
    cmake --build "$build_dir" --target tfx_type_ahead_benchmark --parallel
    "$build_dir/tfx_type_ahead_benchmark" "$name_count" "$iterations"
}

run_benchmark cpp OFF
run_benchmark rust ON
