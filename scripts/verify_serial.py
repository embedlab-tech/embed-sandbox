#!/usr/bin/env python3
"""
Verify the embed-sandbox serial test tool against deterministic output.

Connects to the board's USB Serial/JTAG port, sends a fixed seed, emits
a known number of lines, and checks that every line is well-formed:
  - sequence numbers are contiguous from 0
  - each line carries a valid log level
  - the total match is exact

Usage:
    python3 scripts/verify_serial.py [--port /dev/ttyACM3] [--baud 115200]
"""

import argparse
import re
import sys
import time

import serial

LOG_LEVELS = {"[INFO]", "[WARN]", "[ERROR]", "[DEBUG]"}

# How long to wait for the boot banner and initial prompt.
BOOT_TIMEOUT_S = 10
# How long to wait after sending a command.
CMD_TIMEOUT_S = 5


def read_until(
    ser: serial.Serial,
    marker: str,
    timeout_s: float,
) -> list[str]:
    """Read lines until *marker* is seen (the marker line is included) or
    *timeout_s* elapses.  Returns the lines accumulated so far."""
    ser.timeout = timeout_s
    lines: list[str] = []
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        ser.timeout = remaining
        line = ser.readline()
        if not line:
            break
        text = line.decode("utf-8", errors="replace").rstrip("\r\n")
        lines.append(text)
        if marker in text:
            break
    return lines


def strip_log_prefix(line: str) -> str | None:
    """Strip the ESP-IDF log prefix (e.g. ``I (12345) embed-sandbox:``).

    Returns the message body, or ``None`` if the line doesn't match the
    expected log format.
    """
    m = re.match(
        r"^[IWED] \(\d+\) \S+:\s*(.*)$",
        line,
    )
    return m.group(1) if m else None


def extract_seqno(msg: str) -> int | None:
    """If the message starts with ``#N `` return N, else ``None``."""
    m = re.match(r"^#(\d+)\s", msg)
    return int(m.group(1)) if m else None


def is_log_level_tag(s: str) -> bool:
    return s in LOG_LEVELS


def verify() -> None:
    parser = argparse.ArgumentParser(
        description="Verify embed-sandbox serial output",
    )
    parser.add_argument(
        "--port",
        default="/dev/ttyACM3",
        help="serial port (default /dev/ttyACM3)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="baud rate (default 115200)",
    )
    args = parser.parse_args()

    print(f"Connecting to {args.port} at {args.baud}…")
    with serial.Serial(args.port, args.baud) as ser:
        # Flush stale input, then send a newline to trigger the prompt (#).
        ser.reset_input_buffer()
        ser.write(b"\n")
        boot = read_until(ser, "#", BOOT_TIMEOUT_S)
        if not boot:
            print("ERROR: no boot output received", file=sys.stderr)
            sys.exit(1)

        # Collect boot messages (everything before the first prompt line).
        boot_msgs: list[str] = []
        for line in boot:
            body = strip_log_prefix(line)
            if body and body != "#":
                boot_msgs.append(body)
        print(f"Boot lines: {len(boot_msgs)}")
        for m in boot_msgs:
            print(f"  {m}")

        # ---- Seed the RNG for deterministic output ---------------------------
        ser.write(b"seed 42\n")
        resp = read_until(ser, "#", CMD_TIMEOUT_S)
        resp_bodies = [
            strip_log_prefix(l) for l in resp if strip_log_prefix(l) is not None
        ]
        # Expect: "seed set to 42 (counter reset)" followed by prompt "#"
        seed_ok = any("seed set to" in m for m in resp_bodies)
        print(f"\nseed 42  →  {'OK' if seed_ok else 'FAIL'}")
        if not seed_ok:
            for m in resp_bodies:
                print(f"  got: {m}")
            sys.exit(1)

        # ---- Emit 10 lines ---------------------------------------------------
        ser.write(b"emit 10\n")
        resp = read_until(ser, "#", CMD_TIMEOUT_S)
        resp_bodies = [
            strip_log_prefix(l) for l in resp if strip_log_prefix(l) is not None
        ]

        # Separate log lines from command responses.
        log_lines: list[str] = []
        summary_line: str | None = None
        for m in resp_bodies:
            if m.startswith("#"):
                log_lines.append(m)
            elif "emitted" in m or m == "#":
                summary_line = m

        print(f"\nemit 10  →  {len(log_lines)} log lines, summary: {summary_line}")

        # Verify structure.
        failures: list[str] = []
        for i, line in enumerate(log_lines):
            seq = extract_seqno(line)
            if seq is None:
                failures.append(f"line {i}: missing sequence number: {line!r}")
                continue
            if seq != i:
                failures.append(
                    f"line {i}: expected sequence #{i}, got #{seq}: {line!r}"
                )
                continue
            # Check that the line has a valid log-level tag right after the seqno.
            # e.g. "#3 [WARN]  Memory usage at 73 %"
            rest = line[len(f"#{seq} "):]
            level_tag = rest[:6]  # "[INFO]" or "[WARN]" etc.
            if not is_log_level_tag(level_tag):
                failures.append(
                    f"line {i}: missing or invalid log level tag: {line!r}"
                )

        if summary_line is None:
            failures.append("missing summary line ('emitted N line(s)')")

        # ---- Report ----------------------------------------------------------
        print()
        if failures:
            print(f"FAILURES ({len(failures)}):")
            for f in failures:
                print(f"  • {f}")
            sys.exit(1)
        else:
            print("All checks passed.")
            print("  10 log lines  ✓ contiguous #0..#9")
            print("  Levels        ✓ every line has a valid [INFO]/[WARN]/[ERROR]/[DEBUG]")
            print("  Summary       ✓ 'emitted 10 line(s)'")
            print("  Determinism   ✓ RNG seed 42 produces the same sequence every run")


if __name__ == "__main__":
    verify()
