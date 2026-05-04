#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_TYPE_LOWER="$(printf '%s' "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="${WASM_BUILD_DIR:-${ROOT_DIR}/build-wasm/${BUILD_TYPE_LOWER}}"
PUBLIC_WASM_DIR="${ROOT_DIR}/web/public/wasm"

if ! command -v emcc >/dev/null 2>&1 || ! command -v emcmake >/dev/null 2>&1; then
  EMSDK_DIR="${EMSDK_DIR:-$HOME/.local/emsdk}"

  if [[ ! -f "${EMSDK_DIR}/emsdk_env.sh" ]]; then
    echo "Missing emsdk_env.sh at ${EMSDK_DIR}" >&2
    exit 1
  fi

  # shellcheck disable=SC1090
  source "${EMSDK_DIR}/emsdk_env.sh" >/dev/null
fi

emcmake cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCHINESE_CHESS_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --target chinese_chess_wasm

mkdir -p "${PUBLIC_WASM_DIR}"
cp "${BUILD_DIR}/chinese_chess_wasm.js" "${PUBLIC_WASM_DIR}/"
cp "${BUILD_DIR}/chinese_chess_wasm.wasm" "${PUBLIC_WASM_DIR}/"
cp "${BUILD_DIR}/chinese_chess_wasm.data" "${PUBLIC_WASM_DIR}/"
