#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

installation_hint() {
    cat >&2 <<'EOF'
Required: CMake >= 3.20, a C++17 compiler, Qt 6 Widgets/DBus, and QTermWidget 6.
Qt Test is also required when building tests.
On Ubuntu 26.04, install the development packages with:
  sudo apt-get install -y cmake g++ qt6-base-dev libqtermwidget6-2-dev libutf8proc-dev
Package names and availability may differ on other distributions.
EOF
}

if ! command -v cmake >/dev/null 2>&1; then
    echo "Missing build tool: cmake" >&2
    installation_hint
    exit 1
fi

probe_dir="$(mktemp -d "${TMPDIR:-/tmp}/tfx-dependencies.XXXXXX")"
trap 'rm -rf "$probe_dir"' EXIT

echo "Checking build dependencies (including library linking)..."
if cmake -S "$script_dir/dependency-check" -B "$probe_dir" \
        -DBUILD_TESTING="${1:-OFF}" >"$probe_dir/check.log" 2>&1 \
    && cmake --build "$probe_dir" >>"$probe_dir/check.log" 2>&1; then
    echo "Build dependencies OK."
else
    cat "$probe_dir/check.log" >&2
    echo "Dependency check failed; the application build has not started." >&2
    installation_hint
    exit 1
fi
