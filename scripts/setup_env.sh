#!/usr/bin/env bash
# setup_env.sh — Source this file to activate the repo-local Zephyr environment.
#
# Usage:
#   source scripts/setup_env.sh

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export VENV_DIR="${REPO_ROOT}/toolchains/python/.venv"
export ZEPHYR_BASE="${REPO_ROOT}/third_party/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="${REPO_ROOT}/toolchains/zephyr-sdk-1.0.1"
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"

# External modules
export ZEPHYR_CMSIS_6_CMAKE_DIR="${ZEPHYR_BASE}/modules/cmsis_6"
export ZEPHYR_MODULES="${REPO_ROOT}/third_party/modules/hal/cmsis;${REPO_ROOT}/third_party/modules/hal/hal_stm32;${REPO_ROOT}/third_party/modules/hal/CMSIS_6"

# Prepend venv to PATH
if [[ ":$PATH:" != *":$VENV_DIR/bin:"* ]]; then
    export PATH="$VENV_DIR/bin:$PATH"
fi

echo "[setup_env] Zephyr environment ready"
echo "  ZEPHYR_BASE             = $ZEPHYR_BASE"
echo "  ZEPHYR_SDK_INSTALL_DIR  = $ZEPHYR_SDK_INSTALL_DIR"
echo "  ZEPHYR_TOOLCHAIN_VARIANT = $ZEPHYR_TOOLCHAIN_VARIANT"
echo "  ZEPHYR_MODULES          = $ZEPHYR_MODULES"
echo "  ZEPHYR_CMSIS_6_CMAKE_DIR = $ZEPHYR_CMSIS_6_CMAKE_DIR"
echo "  west                    = $(which west 2>/dev/null || echo 'NOT FOUND')"
