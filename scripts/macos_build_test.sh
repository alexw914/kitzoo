#!/usr/bin/env bash
# Build and test all kitzoo targets on macOS.
# Usage: ./scripts/macos_build_test.sh [debug|release|asan|ubsan|tsan]
# Prerequisites: Xcode Command Line Tools, CMake, Ninja, and Homebrew OpenSSL.

set -euo pipefail

PRESET="${1:-debug}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-macos/$PRESET"

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

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DKITZOO_SANITIZERS="$SANITIZERS" \
    -DKITZOO_BUILD_TESTS=ON -DKITZOO_BUILD_BENCHMARKS=ON -DKITZOO_BUILD_EXAMPLES=ON \
    -DKITZOO_WITH_OPENSSL=ON -DKITZOO_WITH_MIMALLOC=ON \
    -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure --timeout 300
