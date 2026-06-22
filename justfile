# Build, flash, and monitor the embed-sandbox serial test tool.
#
# All I/O is over the USB Serial/JTAG port (the USB-C cable).
# Plug in the board and use `just monitor` to see log output and type commands.

# Show this help.
default:
    @just --list

# Build the firmware (debug).
build:
    cargo build

# Flash the firmware over USB Serial/JTAG.
flash:
    espflash flash target/xtensa-esp32s3-espidf/debug/embed-sandbox

# Build, flash, and open the USB Serial/JTAG console monitor.
run:
    cargo run

# Open the USB Serial/JTAG console monitor (auto-detects port).
monitor:
    espflash monitor

# Open a raw serial terminal (e.g. with a USB-UART adapter).
# Usage:  just serial [port] [baud]
serial port='/dev/ttyUSB0' baud='115200':
    picocom -b {{baud}} {{port}}
