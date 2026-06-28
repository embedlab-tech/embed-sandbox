# embed-sandbox — Justfile for NUCLEO-G070RB Zephyr workflow
#
# The Zephyr environment is activated via `export` inside each recipe.
# This avoids depending on `source scripts/setup_env.sh` being called
# manually before every command.

repo_root := `pwd`

# -- environment helpers (evaluated once per recipe invocation) ---------------

zephyr_base        := repo_root + "/third_party/zephyr"
zephyr_sdk_dir     := repo_root + "/toolchains/zephyr-sdk-1.0.1"
zephyr_modules     := repo_root + "/third_party/modules/hal/cmsis;" \
                    +  repo_root + "/third_party/modules/hal/hal_stm32;" \
                    +  repo_root + "/third_party/modules/hal/CMSIS_6"
zephyr_cmsis6_cmake := zephyr_base + "/modules/cmsis_6"

toolchain_dir      := zephyr_sdk_dir + "/gnu/arm-zephyr-eabi/bin"
size_tool          := toolchain_dir + "/arm-zephyr-eabi-size"

_setup := "export ZEPHYR_BASE="   + zephyr_base        + \
          " ZEPHYR_SDK_INSTALL_DIR=" + zephyr_sdk_dir  + \
          " ZEPHYR_TOOLCHAIN_VARIANT=zephyr"            + \
          " ZEPHYR_CMSIS_6_CMAKE_DIR=" + zephyr_cmsis6_cmake + \
          " ZEPHYR_MODULES=\"" + zephyr_modules + "\""  + \
          " PATH=" + toolchain_dir + ":$PATH"

# -- recipes -----------------------------------------------------------------

# List available commands.
default:
    @just --list --justfile {{justfile()}}

# Bootstrap the repo-local Zephyr environment (one-time).
bootstrap:
    ./scripts/bootstrap_zephyr.sh

# Configure the CMake build tree.
setup:
    {{_setup}} && cmake -B build -DBOARD=nucleo_g070rb

# Incremental firmware build.
build:
    {{_setup}} && cmake --build build

# Full rebuild from scratch.
rebuild:
    rm -rf build
    {{_setup}} && cmake -B build -DBOARD=nucleo_g070rb && cmake --build build

# Flash firmware via ST-Link using west + the stm32flash runner.
flash:
    {{_setup}} && west flash --skip-rebuild --runner stm32flash --device /dev/ttyACM3

# Flash firmware using OpenOCD (many boards ship with onboard ST-Link).
flash-openocd:
    {{_setup}} && west flash --skip-rebuild --runner openocd

# Open serial monitor (ST-LINK VCP, 115200 8N1).
monitor:
    picocom -b 115200 /dev/ttyACM3

# Show firmware section sizes.
size:
    {{_setup}} && {{size_tool}} build/zephyr/zephyr.elf

# Show ROM/RAM usage summary from the last build log.
mem-usage:
    @grep -E "^\s+(FLASH|RAM|SRAM)" build/zephyr/zephyr.elf.map 2>/dev/null || \
        grep -A5 "Memory region" build/zephyr/zephyr.lst 2>/dev/null || \
        grep -A5 "Memory region" build/build.log 2>/dev/null || \
        echo "Run 'just build' first"

# Show generated devicetree (includes overlay results).
dts:
    @less build/zephyr/zephyr.dts 2>/dev/null || echo "Run 'just build' first"

# Show Kconfig configuration summary.
config:
    @less build/zephyr/.config 2>/dev/null || echo "Run 'just build' first"

# Remove build artifacts.
clean:
    rm -rf build

# Remove build artifacts AND third_party (forces full re-clone).
distclean: clean
    rm -rf third_party toolchains

# Verify the bootstrap is healthy (checks SDK, venv, repos).
check:
    ./scripts/bootstrap_zephyr.sh --verify

# Run the serial boot-verification script (board must be connected).
verify-serial:
    python3 scripts/verify_serial.py --port /dev/ttyACM3

# Print all environment variable values used during builds.
dump-env:
    @echo "ZEPHYR_BASE             = {{zephyr_base}}"
    @echo "ZEPHYR_SDK_INSTALL_DIR  = {{zephyr_sdk_dir}}"
    @echo "ZEPHYR_TOOLCHAIN_VARIANT = zephyr"
    @echo "ZEPHYR_MODULES          = {{zephyr_modules}}"
    @echo "ZEPHYR_CMSIS_6_CMAKE_DIR = {{zephyr_cmsis6_cmake}}"
    @echo "PATH extra              = {{toolchain_dir}}"
    @echo "size tool               = {{size_tool}}"
