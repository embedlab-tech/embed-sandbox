//! Serial test tool for the Waveshare ESP32-S3-LCD-1.47.
//!
//! Outputs human-readable random log lines on the USB Serial/JTAG console
//! (visible via `espflash monitor`) at a configurable rate.  An interactive
//! shell on the same port accepts commands to change the output mode.

use std::io::{self, Read};
use std::string::String;
use std::time::{Duration, Instant};

use esp_idf_hal::delay::FreeRtos;
use esp_idf_hal::peripherals::Peripherals;
use log::*;

mod board;

// ---- Tunables ----------------------------------------------------------------

/// Log period in ms per mode.
const MODE_PERIOD_MS: &[(&str, u64)] = &[("fast", 100), ("medium", 500), ("regular", 1000)];

const POLL_MS: u32 = 10;
const LINE_BUF: usize = 80;

// ---- Log line generation ----------------------------------------------------

/// Return a random human-readable log line (without trailing newline).
fn format_log_line(rng: &mut Lcg, n: u64) -> String {
    match rng.range(0, 14) {
        0 => format!(
            "[INFO]  System temperature: {:.1} °C, pressure: {:.1} hPa",
            rng.f32(15.0, 55.0),
            rng.f32(1000.0, 1030.0)
        ),
        1 => format!("[WARN]  Memory usage at {} %", rng.range(50, 99)),
        2 => format!(
            "[INFO]  Sensor read cycle #{} completed in {} ms",
            n,
            rng.range(5, 80)
        ),
        3 => format!(
            "[ERROR] Communication timeout on I2C bus {}",
            rng.range(0, 2)
        ),
        4 => format!(
            "[INFO]  Heart rate: {} BPM, SpO2: {} %",
            rng.range(55, 100),
            rng.range(94, 100)
        ),
        5 => format!(
            "[DEBUG] ADC reading: channel={}, value={}",
            rng.range(0, 7),
            rng.range(0, 4095)
        ),
        6 => format!(
            "[WARN]  Battery voltage: {:.2} V, low threshold: 3.30 V",
            rng.f32(3.0, 4.2)
        ),
        7 => format!(
            "[INFO]  NTP sync completed, offset: {} ms",
            rng.range(-10, 10)
        ),
        8 => format!(
            "[ERROR] CRC mismatch on flash sector {}",
            rng.range(0, 1023)
        ),
        9 => format!(
            "[INFO]  WiFi RSSI: {} dBm, channel: {}",
            rng.range(-90, -30),
            rng.range(1, 13)
        ),
        10 => format!(
            "[INFO]  File system check: {} blocks OK, {} bad",
            rng.range(4000, 4100),
            rng.range(0, 3)
        ),
        11 => format!(
            "[DEBUG] SPI transaction: {} bytes, cs={}, speed={} MHz",
            rng.range(8, 1024),
            rng.range(0, 2),
            rng.range(10, 80)
        ),
        12 => format!(
            "[WARN]  RTC drift: {:.1} ppm, temperature compensated",
            rng.f32(-5.0, 5.0)
        ),
        _ => format!(
            "[INFO]  MQTT publish: topic=sensors/{}, qos={}",
            ["temp", "humidity", "pressure", "light"][rng.range(0, 3) as usize],
            rng.range(0, 2)
        ),
    }
}

/// Emit a log line via the `log` facade (USB Serial/JTAG console).
fn emit_log(rng: &mut Lcg, n: u64) {
    let line = format_log_line(rng, n);

    if line.starts_with("[ERROR]") {
        error!("{}", &line);
    } else if line.starts_with("[WARN]") {
        warn!("{}", &line);
    } else if line.starts_with("[DEBUG]") {
        debug!("{}", &line);
    } else {
        info!("{}", &line);
    }
}

// ---- Minimal LCG random -----------------------------------------------------

struct Lcg(u64);

impl Lcg {
    fn new() -> Self {
        let seed = unsafe { esp_idf_sys::esp_random() as u64 };
        Self(seed.wrapping_add(1))
    }

    fn next_u32(&mut self) -> u32 {
        self.0 = self
            .0
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        (self.0 >> 32) as u32
    }

    /// Uniform random integer in `[lo, hi]` (inclusive).
    fn range(&mut self, lo: i32, hi: i32) -> i32 {
        if hi <= lo {
            return lo;
        }
        lo + (self.next_u32() as i32).rem_euclid(hi - lo + 1)
    }

    /// Uniform random float in `[lo, hi)`.
    fn f32(&mut self, lo: f32, hi: f32) -> f32 {
        lo + (self.next_u32() as f32 / u32::MAX as f32) * (hi - lo)
    }
}

// ---- Shell ------------------------------------------------------------------

/// Try to read one byte from stdin without blocking.
fn poll_stdin() -> Option<u8> {
    let mut buf = [0u8; 1];
    // Setting stdin to non-blocking globally would affect all reads,
    // so use a short timeout via the platform's select/poll.
    // We rely on the fact that ESP-IDF stdin on USB Serial/JTAG returns
    // whatever bytes are buffered without blocking the whole process.
    match io::stdin().read(&mut buf) {
        Ok(0) => None,
        Ok(_) => Some(buf[0]),
        Err(_) => None,
    }
}

fn exec_cmd(
    mode_ms: &mut u64,
    counter: &mut u64,
    line: &str,
) {
    let parts: Vec<&str> = line.split_whitespace().collect();

    match parts.first().copied().unwrap_or("") {
        "mode" => match parts.get(1).copied().unwrap_or("") {
            m @ ("fast" | "medium" | "regular") => {
                *mode_ms = MODE_PERIOD_MS.iter().find(|(n, _)| *n == m).unwrap().1;
                info!("mode set to {} ({} ms interval)", m, mode_ms);
            }
            other => {
                info!("unknown mode '{}'; use fast|medium|regular", other);
            }
        },
        "reset" => {
            *counter = 0;
            info!("log counter reset to 0");
        }
        "help" | "?" => {
            info!(
                "Commands:\n\
                 mode fast|medium|regular    set log output rate\n\
                 reset                     reset log sequence counter\n\
                 help                      this message"
            );
        }
        "" => {}
        _ => {
            info!("unknown command: {} (try `help`)", parts[0]);
        }
    }
}

// ---- Entry point ------------------------------------------------------------

fn main() -> anyhow::Result<()> {
    // The log facade goes to USB Serial/JTAG via EspLogger.
    esp_idf_sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();

    info!("=== embed-sandbox serial test tool ===");
    info!("Type `help` for commands.");

    let _peripherals = Peripherals::take()?;

    // --- State ---------------------------------------------------------------
    let mut rng = Lcg::new();
    let mut mode_ms = MODE_PERIOD_MS[2].1; // "regular"
    let mut counter = 0u64;
    let mut last_log = Instant::now();
    let mut line_buf = [0u8; LINE_BUF];
    let mut line_pos = 0usize;

    // --- Main loop -----------------------------------------------------------
    loop {
        FreeRtos::delay_ms(POLL_MS);

        // Poll for shell input (non-blocking).
        while let Some(byte) = poll_stdin() {
            match byte {
                b'\r' | b'\n' => {
                    let cmd = core::str::from_utf8(&line_buf[..line_pos])
                        .unwrap_or("")
                        .trim()
                        .to_ascii_lowercase();
                    line_pos = 0;
                    if !cmd.is_empty() {
                        exec_cmd(&mut mode_ms, &mut counter, &cmd);
                    }
                    info!("#");
                }
                b'\x7f' | b'\x08' => {
                    if line_pos > 0 {
                        line_pos -= 1;
                    }
                }
                b'\x03' => {
                    line_pos = 0;
                    info!("#");
                }
                32..=126 if line_pos < LINE_BUF - 1 => {
                    line_buf[line_pos] = byte;
                    line_pos += 1;
                }
                _ => {}
            }
        }

        if last_log.elapsed() >= Duration::from_millis(mode_ms) {
            emit_log(&mut rng, counter);
            counter = counter.wrapping_add(1);
            last_log = Instant::now();
        }
    }
}
