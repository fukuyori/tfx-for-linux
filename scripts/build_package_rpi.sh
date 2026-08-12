#!/usr/bin/env bash
set -euo pipefail

# Build Raspberry Pi packages without allowing an unbounded compiler load.
# Tests are opt-in because compiling all Qt Test targets substantially raises
# peak memory use on Raspberry Pi models with limited RAM.

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
build_dir="$repo_root/build-package-rpi"
dist_dir="$repo_root/dist-rpi"
jobs="${TFX_RPI_BUILD_JOBS:-1}"
build_tests="OFF"
clean_first=0
generators=()

usage() {
    cat <<'EOF'
Usage: scripts/build_package_rpi.sh [options]

Builds Raspberry Pi packages into dist-rpi/ with one build job by default.

Options:
  --deb         Build a Debian package (default when dpkg-deb exists)
  --rpm         Build an RPM package (default when rpmbuild exists)
  --tgz         Build a tar.gz archive (always in the default set)
  --tests       Build and run the test suite (disabled by default)
  --jobs N      Run at most N build jobs (default: 1)
  --clean       Remove build-package-rpi/ and dist-rpi/ before building
  -h, --help    Show this help

The default job count can also be set with TFX_RPI_BUILD_JOBS.
For a 4 GB Raspberry Pi, keep N at 1 or 2.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --deb) generators+=("DEB"); shift ;;
        --rpm) generators+=("RPM"); shift ;;
        --tgz) generators+=("TGZ"); shift ;;
        --tests) build_tests="ON"; shift ;;
        --jobs)
            if [[ $# -lt 2 ]]; then
                echo "--jobs requires a positive integer" >&2
                exit 2
            fi
            jobs="$2"
            shift 2
            ;;
        --clean) clean_first=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid job count: $jobs (expected a positive integer)" >&2
    exit 2
fi

architecture="$(uname -m)"
case "$architecture" in
    aarch64|armv6l|armv7l) ;;
    *) echo "warning: this Raspberry Pi package script is running on $architecture" >&2 ;;
esac

if [[ ${#generators[@]} -eq 0 ]]; then
    command -v dpkg-deb >/dev/null 2>&1 && generators+=("DEB")
    command -v rpmbuild >/dev/null 2>&1 && generators+=("RPM")
    generators+=("TGZ")
fi

for generator in "${generators[@]}"; do
    case "$generator" in
        DEB)
            command -v dpkg-deb >/dev/null 2>&1 \
                || { echo "dpkg-deb is required for --deb" >&2; exit 1; }
            ;;
        RPM)
            command -v rpmbuild >/dev/null 2>&1 \
                || { echo "rpmbuild is required for --rpm" >&2; exit 1; }
            ;;
    esac
done

if [[ "$clean_first" -eq 1 ]]; then
    rm -rf "$build_dir" "$dist_dir"
fi

echo "==> Raspberry Pi package build: architecture=$architecture jobs=$jobs tests=$build_tests"
cmake -S "$repo_root/linux-qt" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING="$build_tests"

echo "==> Building"
cmake --build "$build_dir" --parallel "$jobs"

if [[ "$build_tests" == "ON" ]]; then
    echo "==> Running tests sequentially"
    ctest --test-dir "$build_dir" --parallel 1 --output-on-failure
fi

if command -v desktop-file-validate >/dev/null 2>&1; then
    echo "==> Validating desktop entry"
    desktop-file-validate "$repo_root/linux-qt/packaging/tfx.desktop" \
        || echo "warning: desktop entry validation reported issues" >&2
fi

mkdir -p "$dist_dir"
for generator in "${generators[@]}"; do
    echo "==> Packaging ($generator)"
    cpack --config "$build_dir/CPackConfig.cmake" -G "$generator" -B "$dist_dir"
done

# CPack leaves its staging tree behind; only the packages are interesting.
rm -rf "$dist_dir/_CPack_Packages"

echo "==> Packages in dist-rpi/:"
ls -l "$dist_dir"

for deb in "$dist_dir"/*.deb; do
    [[ -e "$deb" ]] || continue
    echo "==> Debian package info: ${deb##*/}"
    dpkg-deb --info "$deb"
done
