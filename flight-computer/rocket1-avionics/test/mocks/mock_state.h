// =============================================================================
// test/mocks/mock_state.h — shared state for all Arduino/driver mocks
//
// The smoke-test harness (firmware_smoke.cpp) writes into this struct to
// control time, I2C presence, sensor readings and SD behaviour; the mock
// headers read from it. This lets the *unmodified* sketch run on a host.
// =============================================================================
#pragma once
#include <cstdint>
#include <set>
#include <string>

struct MockState {
  // --- time ---------------------------------------------------------------
  uint64_t ms = 0; // mock millis(); delay() advances it

  // --- i2c bus ------------------------------------------------------------
  std::set<uint8_t> i2cPresent; // addresses that ACK

  // --- BMP388 -------------------------------------------------------------
  bool baroPresent = true; // begin_I2C() succeeds (if address ACKs too)
  bool baroOk = true;      // performReading() succeeds
  double pressPa = 101325.0;
  double tempC = 15.0;

  // --- ADXL375 ------------------------------------------------------------
  bool accelPresent = true;
  bool accelOk = true;
  float acc[3] = {0.0f, 0.0f, 9.81f};

  // --- SD card ------------------------------------------------------------
  bool sdBeginOk = true;
  bool sdOpenOk = true;
  std::set<std::string> sdExisting; // pre-existing files for SD.exists()
  std::string sdOpenedPath;         // path the sketch actually opened
  std::string sdFileContent;        // everything written to the log file
  int sdFlushes = 0;
};

extern MockState g_mock;
