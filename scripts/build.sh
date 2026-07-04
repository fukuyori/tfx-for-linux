#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
build_dir="$repo_root/build"
build_type="Release"
build_tests="OFF"
run_after_build=0
clean_first=0

usage() {
    cat <<'EOF'
Usage: scripts/build.sh [options]

Options:
  --debug       Build with CMAKE_BUILD_TYPE=Debug
  --release     Build with CMAKE_BUILD_TYPE=Release (default)
  --tests       Also build the Qt Test executables (for ctest)
  --clean       Remove the build directory before configuring
  --run [path]  Run ./build/tfx after building, optionally opening path
  -h, --help    Show this help

The default build skips the test executables so build/ stays small; use
--tests before running "ctest --test-dir build".
EOF
}

run_path=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            build_type="Debug"
            shift
            ;;
        --release)
            build_type="Release"
            shift
            ;;
        --tests)
            build_tests="ON"
            shift
            ;;
        --clean)
            clean_first=1
            shift
            ;;
        --run)
            run_after_build=1
            shift
            if [[ $# -gt 0 && "$1" != --* ]]; then
                run_path="$1"
                shift
            fi
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$clean_first" -eq 1 ]]; then
    rm -rf "$build_dir"
fi

cmake -S "$repo_root/linux-qt" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DBUILD_TESTING="$build_tests"
cmake --build "$build_dir" --parallel

if [[ "$run_after_build" -eq 1 ]]; then
    if [[ -n "$run_path" ]]; then
        "$build_dir/tfx" "$run_path"
    else
        "$build_dir/tfx"
    fi
fi
