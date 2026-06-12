// =============================================================================
// test/mocks/Adafruit_BMP3XX.h — driver mock. Signatures match the real
// Adafruit_BMP3XX library (verified against the actual source) so the sketch
// compiles identically against mock and real driver.
// =============================================================================
#pragma once
#include <cstdint>

#include "Wire.h"
#include "mock_state.h"

#define BMP3_OVERSAMPLING_2X 1
#define BMP3_OVERSAMPLING_4X 2
#define BMP3_IIR_FILTER_COEFF_3 3
#define BMP3_ODR_50_HZ 4

class Adafruit_BMP3XX {
public:
  double pressure = 0.0;    // Pa  (real lib: set by performReading())
  double temperature = 0.0; // degC

  bool begin_I2C(uint8_t addr = 0x77, TwoWire *theWire = nullptr) {
    (void)theWire;
    return g_mock.baroPresent && g_mock.i2cPresent.count(addr) != 0;
  }
  void setTemperatureOversampling(int) {}
  void setPressureOversampling(int) {}
  void setIIRFilterCoeff(int) {}
  void setOutputDataRate(int) {}
  bool performReading() {
    if (!g_mock.baroOk) {
      return false;
    }
    pressure = g_mock.pressPa;
    temperature = g_mock.tempC;
    return true;
  }
};
