 # embed-sandbox

 A deterministic serial test tool for the **Waveshare ESP32-S3-LCD-1.47**
 (ESP32-S3, 16 MB Flash, 8 MB Octal PSRAM).

 Outputs human-readable random log lines on the USB Serial/JTAG console
 (visible via `espflash monitor`) at a configurable rate.  An interactive
 shell on the same port accepts commands to control output.

 The primary use case is testing an external tool that ingests data over a
 serial port: you can start/stop the stream at will, and with a fixed
 RNG seed the same sequence of lines is produced every time.

 ---

 ## Workflow

 Output is **paused by default** on boot.  A typical session:

 ```
 Output is paused. Type `help` for commands, `resume` to start.
 #
 seed 0x1a2b      # lock RNG for deterministic replay
 seed set to 6701 (counter reset)
 #
 rate 10           # 100 lines/second
 rate set to 10 ms
 #
 start             # countdown, then burst 100 lines
 starting in 3..
 2..
 1..
 #0 [ERROR] Communication timeout on I2C bus 1
 #1 [INFO]  Sensor read cycle #1 completed in 12 ms
 #2 [INFO]  System temperature: 38.4 °C, pressure: 1015.3 hPa
 ...
 #99 [WARN]  RTC drift: -1.7 ppm, temperature compensated
 resumed, 100 line(s) emitted
 #100 [INFO]  WiFi RSSI: -54 dBm, channel: 6
 #101 [INFO]  Heart rate: 72 BPM, SpO2: 97 %
 ...
 ```

 Each command responds with a descriptive message.  After `start` the
 100-line burst gives immediate feedback; periodic output continues at
 the configured rate.

 ---

 ## Interactive shell

 While the firmware is running, type commands on the monitor terminal:

 | Command | Description |
 |---|---|
 | `rate <ms>` | Set log interval (minimum 10 ms) |
 | `emit [n]` | Emit n lines now (default 100, max 1000) |
 | `pause` | Stop periodic output |
 | `start` | Countdown 3..2..1.., resume, emit 100 lines |
 | `reset` | Reset counter & RNG to base seed |
 | `seed <n>` | Set RNG seed (decimal or `0x...` hex), reset counter |
 | `status` | Show current state (rate, counter, seed, paused) |
 | `help` | List commands |

 Every log line carries its sequence number as a prefix:

 ```
 #0 [INFO]  System temperature: 24.7 °C, pressure: 1012.3 hPa
 #1 [WARN]  Memory usage at 73 %
 #2 [INFO]  Sensor read cycle #2 completed in 47 ms
 ```

 Because the RNG is deterministic, the same seed + same number of emitted
 lines always produces identical output — an external tool can assert on
 exact log content and sequence numbers.

---

## Prerequisites (macOS)

1. **Rust** (stable is fine for host tooling): <https://rustup.rs>
2. **espup** — installs the Xtensa Rust toolchain + ESP-IDF + LLVM:
   ```bash
   cargo install espup
   ```
3. **espflash / cargo-espflash** — flashing and monitor:
   ```bash
   cargo install espflash cargo-espflash
   ```

### Install the ESP Rust toolchain

```bash
espup install
source ~/export-esp.sh     # do this in every shell that builds the project
```

`espup install` links a toolchain that rustup exposes as `esp`; the project's
[`rust-toolchain.toml`](rust-toolchain.toml) selects it automatically.

> `export-esp.sh` sets environment variables needed by the build. Add
> `source ~/export-esp.sh` to your shell rc (or run it once per terminal) so
> the ESP-IDF build can find its compilers.

---

## Build

```bash
cargo build
```

The first build downloads ESP-IDF, its GCC toolchain, and compiles the SDK —
expect several minutes. Use `cargo build -vv` to watch the progress.

---

## Flash & monitor

```bash
cargo run
```

This builds, flashes, and opens the USB Serial/JTAG monitor in one step.

With an explicit port:

```bash
cargo espflash flash --monitor --port /dev/cu.usbmodem11301
```

### Monitor only (reconnect after a reset)

```bash
espflash monitor
```

### Finding the serial port (macOS)

```bash
ls /dev/cu.*
```

Example output from a known-working board:

```
/dev/cu.usbmodem11301
```

On macOS, prefer the `/dev/cu.*` device over `/dev/tty.*` for flashing.

### Board info

```bash
espflash board-info --port /dev/cu.usbmodem11301
```

### Bootloader mode

If automatic flashing fails (hold BOOT, tap RESET, release BOOT).

---

## Hardware

All board-specific constants live in [`src/board.rs`](src/board.rs).

| Signal           | GPIO |
| ---------------- | ---- |
| BOOT button      | 0    |
| LCD MOSI         | 45   |
| LCD SCLK         | 40   |
| LCD CS           | 42   |
| LCD DC           | 41   |
| LCD RESET        | 39   |
| LCD BACKLIGHT    | 48   |
| RGB LED (WS2812) | 38   |
| MicroSD CMD      | 15   |
| MicroSD CLK      | 14   |
| MicroSD D0       | 16   |
| MicroSD D1       | 18   |
| MicroSD D2       | 17   |
| MicroSD D3       | 21   |

The BOOT button is active-low on GPIO0 — confirm for your board revision.

---

## Crate stack

| Crate             | Version |
| ------------------ | ------- |
| esp-idf-sys        | 0.37    |
| esp-idf-hal        | 0.46    |
| esp-idf-svc        | 0.52    |
| embuild (build)    | 0.33    |

---

## Project layout

```
Cargo.toml            Dependencies + bin target
build.rs              ESP-IDF build entry point (embuild)
rust-toolchain.toml   Pins the project to the "esp" (Xtensa) toolchain
.cargo/config.toml    Target triple, ldproxy linker, espflash runner
sdkconfig.defaults    ESP-IDF defaults (PSRAM, console, flash size)
src/
  main.rs             Log line generator + interactive shell
  board.rs            Board-specific constants
```

---

## Troubleshooting

* **`cargo` says the `esp` toolchain is missing** — run `espup install` and
  `source ~/export-esp.sh`.
* **`error: linker 'ldproxy' not found`** — the `ldproxy` binary is provided
  transitively by the esp-idf-sys build; re-run `source ~/export-esp.sh` so its
  location is on `PATH`.
* **Wrong serial port** — re-check `ls /dev/cu.*`; use the `cu.*` device.
* **Board not entering bootloader mode** — hold BOOT, tap RESET, release BOOT.
* **USB cable is power-only** — many cheap USB-C cables only carry power. Use a
  data/sync cable.
* **`embedded-hal` version mismatch** — if you bump dependencies, keep the
  esp-idf-{sys,hal,svc} trio in lockstep.
