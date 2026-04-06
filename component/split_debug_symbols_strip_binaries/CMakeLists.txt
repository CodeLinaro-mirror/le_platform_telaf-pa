# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

# This module extracts debug symbols and strips binaries after installation
# It must be the last module in the build to ensure all files are installed first

cmake_minimum_required(VERSION 3.10)
project(split_debug_symbols_strip_binaries NONE)

# Install script to extract debug symbols and strip binaries
# This runs after all other component installations
install(SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/split_debug_symbols_strip_binaries.cmake)
