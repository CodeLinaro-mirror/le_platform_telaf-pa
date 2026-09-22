#!/bin/bash

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

set -e

ROOT_DIR=$(dirname "$(readlink -f "$0")")

INSTALL_DIR="${1:-${ROOT_DIR}/build/staging}"
DLT_LOGGING_ARG="${2:-false}"
BUILD_DIR="${ROOT_DIR}/build"

echo ">>> DLT_LOGGING_ARG: ${DLT_LOGGING_ARG}"
echo ">>> cleaning: ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo ">>> Running CMake"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DDLT_LOGGING="${DLT_LOGGING_ARG}"

make -j$(nproc)

echo ">>> Installing to ${INSTALL_DIR}"
# Note: Debug symbol extraction and binary stripping is now handled by CMake install script
make install
echo ">>> Done, output: ${BUILD_DIR}, install: ${INSTALL_DIR}"
