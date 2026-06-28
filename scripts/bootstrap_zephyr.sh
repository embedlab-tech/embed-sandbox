#!/usr/bin/env bash
#
# bootstrap_zephyr.sh — Repo-local Zephyr toolchain + dependency setup
#
# Creates an isolated Zephyr development environment inside this repository.
# Installs only what is needed for the NUCLEO-G070RB target.
#
# Usage:
#   ./scripts/bootstrap_zephyr.sh              # full setup
#   ./scripts/bootstrap_zephyr.sh --verify     # check everything is ready
#
# Environment:
#   REPO_ROOT  — set if running from a different dir (default: repo root)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${REPO_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"

cd "$REPO_ROOT"

# ---- Pinned versions -------------------------------------------------------
ZEPHYR_SDK_VERSION="1.0.1"
ZEPHYR_SDK_BUNDLE="zephyr-sdk-${ZEPHYR_SDK_VERSION}_macos-aarch64_minimal.tar.xz"
ZEPHYR_SDK_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${ZEPHYR_SDK_BUNDLE}"
ZEPHYR_SDK_DIR="${REPO_ROOT}/toolchains/zephyr-sdk-${ZEPHYR_SDK_VERSION}"

ZEPHYR_REPO="https://github.com/zephyrproject-rtos/zephyr"
ZEPHYR_REF="v4.4.1"

CMSIS_REPO="https://github.com/zephyrproject-rtos/cmsis"
CMSIS_REF="512cc7e895e8491696b61f7ba8066b4a182569b8"

HAL_STM32_REPO="https://github.com/zephyrproject-rtos/hal_stm32"
HAL_STM32_REF="ff19fb0b03bc9b3f1355e9f9b7b6b3a6c051f121"

CMSIS_6_REPO="https://github.com/zephyrproject-rtos/CMSIS_6"
CMSIS_6_REF="30a859f44ef8ab4dc8f84b03ed586fd16ccf9d74"

VENV_DIR="${REPO_ROOT}/toolchains/python/.venv"
THIRD_PARTY_DIR="${REPO_ROOT}/third_party"
ZEPHYR_BASE="${THIRD_PARTY_DIR}/zephyr"

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[bootstrap]${NC} $1"; }
warn()  { echo -e "${YELLOW}[bootstrap]${NC} $1"; }
error() { echo -e "${RED}[bootstrap]${NC} $1"; }

# ---- Verify mode -----------------------------------------------------------
if [ "${1:-}" = "--verify" ]; then
    info "Verifying bootstrap environment..."
    failures=0

    [ ! -f "$VENV_DIR/bin/activate" ] && { error "Python venv not found"; failures=$((failures+1)); } || info "Python venv: OK"
    "$VENV_DIR/bin/west" --version &>/dev/null 2>&1 || { error "west not installed"; failures=$((failures+1)); } || info "west: OK"
    [ ! -d "$ZEPHYR_SDK_DIR" ] && { error "Zephyr SDK not found"; failures=$((failures+1)); } || info "Zephyr SDK: OK"
    [ ! -d "$ZEPHYR_BASE/.git" ] && { error "Zephyr repo not cloned"; failures=$((failures+1)); } || info "Zephyr repo: OK"
    [ ! -d "${THIRD_PARTY_DIR}/modules/hal/cmsis/.git" ] && { warn "CMSIS module not cloned"; } || info "CMSIS module: OK"
    [ ! -d "${THIRD_PARTY_DIR}/modules/hal/hal_stm32/.git" ] && { error "STM32 HAL not cloned"; failures=$((failures+1)); } || info "STM32 HAL: OK"
    [ ! -d "${THIRD_PARTY_DIR}/modules/hal/CMSIS_6/.git" ] && { error "CMSIS_6 not cloned"; failures=$((failures+1)); } || info "CMSIS_6: OK"

    if [ "$failures" -eq 0 ]; then info "All checks passed."; else error "${failures} check(s) failed."; fi
    exit "$failures"
fi

# ---- Main bootstrap --------------------------------------------------------
info "Starting repo-local Zephyr bootstrap in $REPO_ROOT"

# 1. Python venv
if [ ! -f "$VENV_DIR/bin/activate" ]; then
    info "Creating Python virtual environment..."
    python3 -m venv "$VENV_DIR"
fi
"$VENV_DIR/bin/pip" install --upgrade pip --quiet 2>/dev/null || true

# 2. Install west
if ! "$VENV_DIR/bin/west" --version &>/dev/null 2>&1; then
    info "Installing west..."
    "$VENV_DIR/bin/pip" install west --quiet 2>&1
fi

# 3. Zephyr SDK (minimal bundle + ARM toolchain only)
if [ ! -d "$ZEPHYR_SDK_DIR" ]; then
    info "Downloading Zephyr SDK minimal bundle..."
    mkdir -p "$(dirname "$ZEPHYR_SDK_DIR")"
    cd "$(dirname "$ZEPHYR_SDK_DIR")"
    curl -L -O "$ZEPHYR_SDK_URL"
    info "Extracting SDK..."
    tar xf "$ZEPHYR_SDK_BUNDLE"
    rm "$ZEPHYR_SDK_BUNDLE"

    ARM_TOOLCHAIN="toolchain_gnu_macos-aarch64_arm-zephyr-eabi.tar.xz"
    ARM_TOOLCHAIN_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${ARM_TOOLCHAIN}"
    cd "$ZEPHYR_SDK_DIR"
    info "Downloading ARM toolchain..."
    curl -L -O "$ARM_TOOLCHAIN_URL"
    info "Extracting ARM toolchain..."
    tar xf "$ARM_TOOLCHAIN"
    rm "$ARM_TOOLCHAIN"
    # Move to gnu/ subdirectory as expected by SDK cmake
    mkdir -p gnu && mv arm-zephyr-eabi gnu/
    cd "$REPO_ROOT"
else
    info "Zephyr SDK already present"
fi

# 4. Clone Zephyr
if [ ! -d "$ZEPHYR_BASE/.git" ]; then
    info "Cloning Zephyr ($ZEPHYR_REF)..."
    mkdir -p "$THIRD_PARTY_DIR"
    git clone --depth 1 --branch "$ZEPHYR_REF" "$ZEPHYR_REPO" "$ZEPHYR_BASE"
else
    info "Zephyr repo already cloned"
fi

# 5. Clone CMSIS (old)
if [ ! -d "${THIRD_PARTY_DIR}/modules/hal/cmsis/.git" ]; then
    info "Cloning CMSIS module..."
    mkdir -p "${THIRD_PARTY_DIR}/modules/hal"
    git clone --depth 1 "$CMSIS_REPO" "${THIRD_PARTY_DIR}/modules/hal/cmsis"
fi

# 6. Clone STM32 HAL
if [ ! -d "${THIRD_PARTY_DIR}/modules/hal/hal_stm32/.git" ]; then
    info "Cloning STM32 HAL module..."
    mkdir -p "${THIRD_PARTY_DIR}/modules/hal"
    git clone --depth 1 "$HAL_STM32_REPO" "${THIRD_PARTY_DIR}/modules/hal/hal_stm32"
fi

# 7. Clone CMSIS_6 (needed for Cortex-M CMSIS support)
if [ ! -d "${THIRD_PARTY_DIR}/modules/hal/CMSIS_6/.git" ]; then
    info "Cloning CMSIS_6 module..."
    mkdir -p "${THIRD_PARTY_DIR}/modules/hal"
    git clone --depth 1 "$CMSIS_6_REPO" "${THIRD_PARTY_DIR}/modules/hal/CMSIS_6"
    # Check out the pinned revision
    cd "${THIRD_PARTY_DIR}/modules/hal/CMSIS_6"
    git fetch --depth 1 origin "$CMSIS_6_REF" 2>/dev/null || true
    git checkout "$CMSIS_6_REF" 2>/dev/null || true
    cd "$REPO_ROOT"
fi

# 8. Generate setup_env.sh
info "Writing scripts/setup_env.sh..."
cat > "${REPO_ROOT}/scripts/setup_env.sh" << SETUP_EOF
#!/usr/bin/env bash
# setup_env.sh — Source this file to activate the repo-local Zephyr environment.
#
# Usage:
#   source scripts/setup_env.sh

REPO_ROOT="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")/.." && pwd)"

export VENV_DIR="\${REPO_ROOT}/toolchains/python/.venv"
export ZEPHYR_BASE="\${REPO_ROOT}/third_party/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="\${REPO_ROOT}/toolchains/zephyr-sdk-${ZEPHYR_SDK_VERSION}"
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
export ZEPHYR_CMSIS_6_CMAKE_DIR="\${ZEPHYR_BASE}/modules/cmsis_6"
export ZEPHYR_MODULES="\${REPO_ROOT}/third_party/modules/hal/cmsis;\${REPO_ROOT}/third_party/modules/hal/hal_stm32;\${REPO_ROOT}/third_party/modules/hal/CMSIS_6"

if [[ ":\$PATH:" != *":\$VENV_DIR/bin:"* ]]; then
    export PATH="\$VENV_DIR/bin:\$PATH"
fi

echo "[setup_env] Zephyr environment ready"
echo "  ZEPHYR_BASE             = \$ZEPHYR_BASE"
echo "  ZEPHYR_SDK_INSTALL_DIR  = \$ZEPHYR_SDK_INSTALL_DIR"
echo "  ZEPHYR_TOOLCHAIN_VARIANT = \$ZEPHYR_TOOLCHAIN_VARIANT"
echo "  ZEPHYR_MODULES          = \$ZEPHYR_MODULES"
echo "  ZEPHYR_CMSIS_6_CMAKE_DIR = \$ZEPHYR_CMSIS_6_CMAKE_DIR"
echo "  west                    = \$(which west 2>/dev/null || echo 'NOT FOUND')"
SETUP_EOF
chmod +x "${REPO_ROOT}/scripts/setup_env.sh"

info ""
info "Bootstrap complete!"
info "Next: source scripts/setup_env.sh && cmake -B build -DBOARD=nucleo_g070rb && cmake --build build"
info "To verify: ./scripts/bootstrap_zephyr.sh --verify"
