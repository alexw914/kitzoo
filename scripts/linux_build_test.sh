#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build and test kitzoo on a Linux host.
#
# Usage:
#   ./scripts/linux_build_test.sh [preset]     # default preset: debug
#
# Examples:
#   ./scripts/linux_build_test.sh debug
#   ./scripts/linux_build_test.sh asan
#   ./scripts/linux_build_test.sh tsan
# ---------------------------------------------------------------------------
set -euo pipefail

PRESET="${1:-debug}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

case "$PRESET" in
    asan)  SANITIZERS="address" ;;
    ubsan) SANITIZERS="undefined" ;;
    tsan)  SANITIZERS="thread" ;;
    *)     SANITIZERS="" ;;
esac

if [ "$PRESET" = "release" ]; then
    BUILD_TYPE="Release"
else
    BUILD_TYPE="Debug"
fi

cmake -S "$ROOT" -B "$ROOT/build-linux/$PRESET" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DKITZOO_SANITIZERS="$SANITIZERS" \
    -DKITZOO_BUILD_TESTS=ON -DKITZOO_BUILD_BENCHMARKS=OFF -DKITZOO_BUILD_EXAMPLES=OFF \
    -DKITZOO_WITH_OPENSSL=ON
cmake --build "$ROOT/build-linux/$PRESET"
ctest --test-dir "$ROOT/build-linux/$PRESET" --output-on-failure --timeout 300
