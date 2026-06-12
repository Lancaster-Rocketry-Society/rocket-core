// =============================================================================
// LURS Rocket 1 — flight data logger
// config.h — hardware wiring + logging configuration
//
// CHECK THIS FILE AGAINST THE ACTUAL BOARD BEFORE FLASHING.
// Everything hardware-specific lives here. Flight-detection tunables live in
// FlightTunables (flight_logic.h).
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
// Pins (Arduino pin numbering — select the correct board in the IDE:
// "Arduino Nano ESP32" or "Arduino Nano 33 IoT". Pin labels are identical.)
//
//   I2C  : BMP388 + ADXL375 share the bus on A4 (SDA) / A5 (SCL)
//   SPI  : microSD module on D11 (COPI/MOSI), D12 (CIPO/MISO), D13 (SCK)
// ---------------------------------------------------------------------------
#define PIN_SD_CS 10 // microSD chip-select -> D10. CHANGE IF WIRED DIFFERENTLY.

// ---------------------------------------------------------------------------
// I2C addresses. The firmware tries PRIMARY then SECONDARY automatically and
// reports what it found over Serial and in the log file header, so a wrong
// solder-jumper/SDO level is detected rather than silently failing.
// ---------------------------------------------------------------------------
#define BMP388_ADDR_PRIMARY 0x77
#define BMP388_ADDR_SECONDARY 0x76
#define ADXL375_ADDR_PRIMARY 0x53
#define ADXL375_ADDR_SECONDARY 0x1D

// ---------------------------------------------------------------------------
// Loop & logging
// ---------------------------------------------------------------------------
#define LOOP_PERIOD_MS 10        // 100 Hz main loop
#define LOG_FLUSH_MS 1000        // flush file to card at least this often
#define LOG_WRITE_THRESHOLD 1024 // write buffer to file once this full (bytes)
#define LOG_BUFFER_BYTES 1600    // RAM log buffer (no heap, fixed size)
#define LANDED_ROW_PERIOD_MS 500 // after landing, log at 2 Hz until power-off
#define SERIAL_BAUD 115200
#define SERIAL_WAIT_MS 2000 // max wait for USB serial — must boot headless

#define FIRMWARE_VERSION "rocket1-logger v1.0.0"
