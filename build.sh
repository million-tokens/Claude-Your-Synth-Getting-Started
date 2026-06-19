#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"

PARALLEL_JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel "${PARALLEL_JOBS}"

BINARY=""
for candidate in \
    "${BUILD_DIR}/JuceMinimal_artefacts/Release/JuceMinimal" \
    "${BUILD_DIR}/JuceMinimal_artefacts/Debug/JuceMinimal" \
    "${BUILD_DIR}/JuceMinimal" \
    "${BUILD_DIR}/Release/JuceMinimal"; do
    if [[ -x "${candidate}" ]]; then
        BINARY="${candidate}"
        break
    fi
done

if [[ -z "${BINARY}" ]]; then
    echo "Build complete. Binary not found in expected locations."
    exit 1
fi

echo ""
echo "Running: ${BINARY}"
"${BINARY}"
