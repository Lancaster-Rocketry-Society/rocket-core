// =============================================================================
// test/mocks/Wire.h — I2C mock. endTransmission() ACKs only the addresses
// listed in g_mock.i2cPresent, so the sketch's i2c scan and address fallback
// logic is exercised for real.
// =============================================================================
#pragma once
#include <cstdint>

#include "mock_state.h"

struct TwoWire {
  uint8_t last = 0;
  void begin() {}
  void setClock(unsigned long) {}
  void beginTransmission(uint8_t a) { last = a; }
  uint8_t endTransmission() {
    return g_mock.i2cPresent.count(last) ? 0 : 2; // 0 = ACK, 2 = addr NACK
  }
};

extern TwoWire Wire;
