#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BUILD_TYPE="${BUILD_TYPE:-Release}"
OUTPUT_FOLDER="${OUTPUT_FOLDER:-${ROOT_DIR}/build}"

echo "=== Building (${BUILD_TYPE}) ==="
echo "ROOT_DIR=${ROOT_DIR}"
echo "OUTPUT=${OUTPUT_FOLDER}"

cmake -S "${ROOT_DIR}" \
      -B "${OUTPUT_FOLDER}/build/${BUILD_TYPE}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${OUTPUT_FOLDER}/build/${BUILD_TYPE}" \
      --parallel "$(nproc)"

echo "=== Build complete ==="
echo "csv_to_columnar:   ${OUTPUT_FOLDER}/build/${BUILD_TYPE}/exe/csv_to_columnar"
echo "ngn-clickbench-run: ${OUTPUT_FOLDER}/build/${BUILD_TYPE}/clickbench/ngn-clickbench-run"
