#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIKAFISH_DIR="${ROOT_DIR}/third_party/pikafish"
PIKAFISH_SRC_DIR="${PIKAFISH_DIR}/src"
PIKAFISH_ARCH="${PIKAFISH_ARCH:-x86-64-avx2}"
JOBS="${JOBS:-$(nproc)}"

if [[ ! -d "${PIKAFISH_DIR}/.git" ]]; then
  echo "Initializing Pikafish submodule..."
  git -C "${ROOT_DIR}" submodule update --init --depth 1 third_party/pikafish
fi

if [[ ! -d "${PIKAFISH_SRC_DIR}" ]]; then
  echo "Missing Pikafish source directory: ${PIKAFISH_SRC_DIR}" >&2
  exit 1
fi

echo "Building Pikafish with ARCH=${PIKAFISH_ARCH} JOBS=${JOBS}"
make -C "${PIKAFISH_SRC_DIR}" -j"${JOBS}" build ARCH="${PIKAFISH_ARCH}"
