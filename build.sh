#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
PLUGIN_NAME="MiniSynth.component"
INSTALL_DIR="${HOME}/Library/Audio/Plug-Ins/Components"

PARALLEL_JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel "${PARALLEL_JOBS}" --target MiniSynth_AU

if [[ ! -d "${INSTALL_DIR}/${PLUGIN_NAME}" ]]; then
    echo "Build complete, but the plugin is not installed in ${INSTALL_DIR}."
    exit 1
fi

echo ""
echo "Installed: ${INSTALL_DIR}/${PLUGIN_NAME}"
