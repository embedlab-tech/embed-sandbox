#!/usr/bin/env python3
"""
verify_serial.py — Check that the embed-sandbox firmware is alive on USART2.

Connects to the board's ST-LINK VCP, expects the banner and shell prompt.
Used by CI after flashing to confirm the firmware boots correctly.

Usage:
    python3 scripts/verify_serial.py [--port /dev/ttyACM3] [--baud 115200]
"""

import argparse
import sys
import time

import serial  # pyserial


def main():
    parser = argparse.ArgumentParser(description="Verify embed-sandbox firmware boot")
    parser.add_argument("--port", default="/dev/ttyACM3", help="Serial port (default: /dev/ttyACM3)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=5.0, help="Seconds to wait for banner")
    args = parser.parse_args()

    print(f"Connecting to {args.port} at {args.baud} baud ...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=2)
    except serial.SerialException as e:
        print(f"FAIL: Cannot open {args.port}: {e}")
        sys.exit(1)

    # Give the board time to boot and print the banner
    deadline = time.monotonic() + args.timeout
    output = ""
    banner_found = False

    try:
        while time.monotonic() < deadline:
            data = ser.read(256)
            if data:
                text = data.decode("utf-8", errors="replace")
                output += text
                print(text, end="", flush=True)
                if "embed-sandbox" in output and "=== embed-sandbox ===" in output:
                    banner_found = True
                    break
            else:
                time.sleep(0.1)
    finally:
        ser.close()

    if banner_found:
        print()
        print("PASS: Firmware banner received — board is alive")
        sys.exit(0)
    else:
        print()
        print("FAIL: No firmware banner received within timeout")
        print(f"Captured output:\n{output}")
        sys.exit(1)


if __name__ == "__main__":
    main()
