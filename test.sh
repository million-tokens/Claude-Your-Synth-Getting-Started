#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"

PARALLEL_JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel "${PARALLEL_JOBS}" --target MiniSynthTests

ctest --test-dir "${BUILD_DIR}" --build-config Release --output-on-failure
