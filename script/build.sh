#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
OUTPUT_FOLDER="${OUTPUT_FOLDER:-${ROOT_DIR}/build}"

# Папка куда cmake будет собирать
# Итоговая структура:
# build/
# ├── exe/
# │   └── csv_to_columnar
# └── clickbench/
#     └── ngn-clickbench-run

BUILD_DIR="${OUTPUT_FOLDER}/build/${BUILD_TYPE}"

echo "=== Building (${BUILD_TYPE}) ==="
echo "ROOT_DIR=${ROOT_DIR}"
echo "BUILD_DIR=${BUILD_DIR}"

# Удаляем старую сборку если есть
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# cmake -B = папка куда идут артефакты сборки (CMAKE_BINARY_DIR)
# Бинарники будут в CMAKE_BINARY_DIR/exe/ и CMAKE_BINARY_DIR/clickbench/
cmake -S "${ROOT_DIR}" \
      -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${BUILD_DIR}" \
      --parallel "$(nproc)"

echo "=== Build complete ==="
echo "csv_to_columnar:    ${BUILD_DIR}/exe/csv_to_columnar"
echo "ngn-clickbench-run: ${BUILD_DIR}/clickbench/ngn-clickbench-run"

