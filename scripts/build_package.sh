#!/usr/bin/env bash
set -euo pipefail

# Build distributable packages for tfx-for-linux.
#
# Configures a clean Release build, runs the test suite, and produces
# packages with CPack into dist/. Generators are auto-detected: DEB when
# dpkg-deb is available, RPM when rpmbuild is available, and a tar.gz always.

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
build_dir="$repo_root/build-package"
dist_dir="$repo_root/dist"

usage() {
    cat <<'EOF'
Usage: scripts/build_package.sh [options]

Builds installation packages into dist/.

Options:
  --deb         Build a Debian package (default when dpkg-deb exists)
  --rpm         Build an RPM package (default when rpmbuild exists)
  --tgz         Build a tar.gz archive (always in the default set)
  --clean       Remove build-package/ and dist/ before building
  --skip-tests  Do not run ctest before packaging
  -h, --help    Show this help
EOF
}

generators=()
clean_first=0
skip_tests=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --deb) generators+=("DEB"); shift ;;
        --rpm) generators+=("RPM"); shift ;;
        --tgz) generators+=("TGZ"); shift ;;
        --clean) clean_first=1; shift ;;
        --skip-tests) skip_tests=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ${#generators[@]} -eq 0 ]]; then
    command -v dpkg-deb >/dev/null 2>&1 && generators+=("DEB")
    command -v rpmbuild >/dev/null 2>&1 && generators+=("RPM")
    generators+=("TGZ")
fi

if [[ "$clean_first" -eq 1 ]]; then
    rm -rf "$build_dir" "$dist_dir"
fi

echo "==> Configuring Release build"
cmake -S "$repo_root/linux-qt" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

echo "==> Building"
cmake --build "$build_dir" --parallel

if [[ "$skip_tests" -eq 0 ]]; then
    echo "==> Running tests"
    ctest --test-dir "$build_dir" --output-on-failure
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

echo "==> Packages in dist/:"
ls -l "$dist_dir"

for deb in "$dist_dir"/*.deb; do
    [[ -e "$deb" ]] || continue
    echo "==> Debian package info: ${deb##*/}"
    dpkg-deb --info "$deb"
done
