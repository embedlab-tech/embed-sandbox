# embed-sandbox

A Rust + ESP-IDF scaffold for the **Waveshare ESP32-S3-LCD-1.47**
(ESP32-S3, 16 MB Flash, 8 MB Octal PSRAM, 1.47" 172x320 ST7789 SPI display).

The first milestone is intentionally small: turn the display on, render a demo
screen with `embedded-graphics`, read the onboard BOOT button, and react to
short / long presses. MicroSD, the QMI8658 IMU, Wi-Fi and BLE are deliberately
left out (see [Out of scope](#out-of-scope)).

---

## Hardware map

All of this lives in [`src/board.rs`](src/board.rs) - change the board by
editing that one file.

| Signal           | GPIO |
| ---------------- | ---- |
| LCD MOSI         | 45   |
| LCD SCLK         | 40   |
| LCD CS           | 42   |
| LCD DC           | 41   |
| LCD RESET        | 39   |
| LCD BACKLIGHT    | 48   |
| BOOT button      | 0    |
| RGB LED (WS2812) | 38   |
| MicroSD CMD      | 15   |
| MicroSD CLK      | 14   |
| MicroSD D0       | 16   |
| MicroSD D1       | 18   |
| MicroSD D2       | 17   |
| MicroSD D3       | 21   |

> **Note on the button:** the BOOT button on ESP32-S3 Waveshare LCD boards is
> wired to GPIO0 and is active-low. This is the standard ESP32-S3 BOOT pin, but
> **please confirm it for your exact board revision** (see the `TODO(board)`
> in `src/board.rs`). If it differs, change `BUTTON_GPIO` and the single
> `peripherals.pins.gpio0` call site in `src/main.rs`.

---

## Crate stack

These are released in lockstep by the esp-rs team and target `embedded-hal` 1.0:

| Crate             | Version |
| ------------------ | ------- |
| esp-idf-sys        | 0.37    |
| esp-idf-hal        | 0.46    |
| esp-idf-svc        | 0.52    |
| mipidsi            | 0.10    |
| embedded-graphics  | 0.8     |
| embuild (build)    | 0.33    |

`mipidsi` 0.10 bundles its own SPI interface, so no separate
`display-interface-spi` crate is required.

---

## Prerequisites (macOS)

1. **Rust** (stable is fine for the host tooling): <https://rustup.rs>
2. **espup** - installs the Xtensa Rust toolchain + ESP-IDF + LLVM:
   ```bash
   cargo install espup
   ```
3. **cargo-generate** - optional, only if you want to scaffold more projects
   from the official template:
   ```bash
   cargo install cargo-generate
   ```
4. **espflash / cargo-espflash** - the primary flashing tools:
   ```bash
   cargo install espflash cargo-espflash
   ```

`probe-rs` is **optional** (JTAG debugging) and documented separately below.

### Install the ESP Rust toolchain

```bash
espup install
source ~/export-esp.sh     # do this in every shell that builds the project
```

`espup install` links a toolchain that rustup exposes as `esp`; the project's
[`rust-toolchain.toml`](rust-toolchain.toml) selects it automatically. If
`cargo` complains the `esp` toolchain is missing, you forgot the two steps
above.

> `export-esp.sh` sets environment variables needed by the build. Add
> `source ~/export-esp.sh` to your shell rc (or run it once per terminal) so
> the ESP-IDF build can find its compilers.

---

## Build

```bash
cargo build
```

The first build downloads ESP-IDF, its GCC toolchain, and compiles the SDK -
expect several minutes. Run with `cargo build -vv` to watch that progress.

---

## Flash

The board flashes over the ESP32-S3 ROM bootloader (USB). The recommended
workflow is **cargo-espflash**:

```bash
cargo espflash flash --monitor
```

With an explicit port:

```bash
cargo espflash flash --monitor --port /dev/cu.usbmodem11301
```

`--monitor` keeps the serial monitor open after flashing so you can see
`log::info!` output (and the heartbeat).

### Build only, then flash the binary directly

```bash
cargo build
espflash flash target/xtensa-esp32s3-espidf/debug/embed-sandbox
espflash monitor
```

---

## Monitor

```bash
espflash monitor
# or, attached to a specific port:
espflash monitor --port /dev/cu.usbmodem11301
```

Logs are routed to the USB Serial/JTAG interface via
`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in
[`sdkconfig.defaults`](sdkconfig.defaults), which is the same port the monitor
reads.

### Finding the serial port (macOS)

```bash
ls /dev/cu.*
```

Example output from a known-working board:

```
/dev/cu.usbmodem11301
```

On macOS, prefer the `/dev/cu.*` device over `/dev/tty.*` for flashing.
`/dev/cu.*` is the callout device that espflash opens for write access;
`/dev/tty.*` can block on incoming call handling.

### Board info

```bash
espflash board-info --port /dev/cu.usbmodem11301
```

### Bootloader mode

If automatic flashing fails (e.g. `espflash` cannot reset the chip), put the
board into bootloader mode manually: hold **BOOT**, press and release
**RESET**, then release **BOOT**. Then retry the flash command.

---

## Optional workflow: probe-rs (JTAG debugging)

`probe-rs` is **not required** for flashing; use `espflash` for that. probe-rs
is recommended later for interactive, source-level debugging over the ESP32-S3
USB-JTAG interface.

Install:

```bash
cargo install probe-rs-tools
```

Commands:

```bash
probe-rs list
probe-rs info  --chip esp32s3
probe-rs run   --chip esp32s3 target/xtensa-esp32s3-espidf/debug/embed-sandbox
```

Caveats:

* probe-rs requires the ESP32-S3 **USB-JTAG** interface, whose availability
  depends on the board's USB wiring and firmware.
* If probe-rs cannot detect the target, fall back to **espflash** - it should
  always work over the ROM bootloader.
* espflash remains the primary flashing workflow; probe-rs is for debugging.

---

## Project layout

```
Cargo.toml            Dependencies + bin target
build.rs              ESP-IDF build entry point (embuild)
rust-toolchain.toml   Pins the project to the "esp" (Xtensa) toolchain
.cargo/config.toml    Target triple, ldproxy linker, espflash runner
sdkconfig.defaults    ESP-IDF defaults (PSRAM, console, flash size)
src/
  main.rs             Orchestration only: init display + button, run loop
  board.rs            ALL hardware constants (pins, geometry, tuning)
  display.rs          SPI + ST7789 init, mipidsi Display, demo screen
  button.rs           Debounced input + press / long-press events
```

---

## Out of scope (for now)

The following are intentionally **not** implemented in this first milestone.
Pin assignments are documented in `src/board.rs` for future use:

* MicroSD card + filesystem
* QMI8658 IMU
* Wi-Fi / BLE
* RGB LED (WS2812) - pin documented only

---

## Troubleshooting

### Build / flash

* **`cargo` says the `esp` toolchain is missing** - run `espup install` and
  `source ~/export-esp.sh`.
* **`error: linker 'ldproxy' not found`** - the `ldproxy` binary is provided
  transitively by the esp-idf-sys build; re-run `source ~/export-esp.sh` so its
  location is on `PATH`.
* **Wrong serial port** - re-check `ls /dev/cu.*`; use the `cu.*` device.
* **Board not entering bootloader mode** - hold BOOT, tap RESET, release BOOT.
* **USB cable is power-only** - many cheap USB-C cables only carry power. Use a
  data/sync cable.
* **`embedded-hal` version mismatch** - the crate versions above are a matched
  set; do not mix in an older `mipidsi` or `st7789` crate. If you change one,
  bump the whole esp-idf-{sys,hal,svc} trio together.
* **probe-rs cannot detect USB-JTAG** - fall back to espflash.

### Display

* **Black screen** - backlight not enabled. It is driven high in
  `display::init_display`; check `LCD_BACKLIGHT_GPIO` (48) and its polarity
  (`TODO(tuning)`).
* **Backlight off when it should be on (or vice-versa)** - invert the
  `backlight.set_high()` call; backlight polarity varies by board.
* **Wrong image position / clipped** - `DISPLAY_OFFSET_X/Y` in `board.rs` are
  wrong. For a 172-wide panel in a 240-wide framebuffer the horizontal offset
  is `(240-172)/2 = 34`; in landscape the pair is often `(0, 34)`.
* **Image rotated 90 / 180** - change `DISPLAY_LANDSCAPE` or the
  `Rotation::Deg90` setting in `display.rs`.
* **Red and blue swapped** - flip `ColorOrder::Rgb` to `ColorOrder::Bgr`.
* **Colors inverted (negative image)** - toggle `invert_colors` on the mipidsi
  builder.
* **SPI glitches / tearing** - lower `LCD_SPI_FREQ_MHZ` (e.g. to 20).
* **Garbage pixels** - confirm `data_mode(MODE_0)`; some panels need `MODE_3`.
* **Onboard button does nothing / always reads pressed** - confirm the BOOT
  button GPIO (`BUTTON_GPIO`); confirm active-low polarity
  (`BUTTON_ACTIVE_LOW`).
