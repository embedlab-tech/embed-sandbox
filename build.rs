//! ESP-IDF build integration.
//!
//! `embuild::espidf::sysenv::output()` is the standard esp-idf-template entry
//! point: it lets the `esp-idf-sys` build script drive the ESP-IDF SDK build
//! (download, configure, compile) and emits the cargo/linker instructions the
//! rest of the crate needs. No manual ESP-IDF clone or C toolchain required.

fn main() {
    embuild::espidf::sysenv::output();
}
