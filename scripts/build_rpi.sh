#!/usr/bin/env bash
set -euo pipefail

# Resource-conscious build for Raspberry Pi.
# Keep the default at one compiler process: Qt/C++ compilation can exhaust
# memory on smaller Raspberry Pi models when all cores are used at once.

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
build_dir="$repo_root/build-rpi"
build_type="Release"
build_tests="OFF"
jobs="${TFX_RPI_BUILD_JOBS:-1}"
run_after_build=0
clean_first=0
run_path=""

usage() {
    cat <<'EOF'
Usage: scripts/build_rpi.sh [options]

Builds tfx in build-rpi/ with Raspberry Pi-safe defaults.

Options:
  --debug       Build with CMAKE_BUILD_TYPE=Debug
  --release     Build with CMAKE_BUILD_TYPE=Release (default)
  --tests       Also build the Qt Test executables (disabled by default)
  --jobs N      Run at most N build jobs (default: 1)
  --clean       Remove build-rpi/ before configuring
  --run [path]  Run ./build-rpi/tfx after building, optionally opening path
  -h, --help    Show this help

The default job count can also be set with TFX_RPI_BUILD_JOBS.
EOF
}

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
        --jobs)
            if [[ $# -lt 2 ]]; then
                echo "--jobs requires a positive integer" >&2
                exit 2
            fi
            jobs="$2"
            shift 2
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

if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid job count: $jobs (expected a positive integer)" >&2
    exit 2
fi

architecture="$(uname -m)"
case "$architecture" in
    aarch64|armv6l|armv7l) ;;
    *) echo "warning: this Raspberry Pi build script is running on $architecture" >&2 ;;
esac

if [[ "$clean_first" -eq 1 ]]; then
    rm -rf "$build_dir"
fi

echo "==> Raspberry Pi build: architecture=$architecture jobs=$jobs tests=$build_tests"
cmake -S "$repo_root/linux-qt" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DBUILD_TESTING="$build_tests"

cmake --build "$build_dir" --parallel "$jobs"

if [[ "$run_after_build" -eq 1 ]]; then
    if [[ -n "$run_path" ]]; then
        "$build_dir/tfx" "$run_path"
    else
        "$build_dir/tfx"
    fi
fi
