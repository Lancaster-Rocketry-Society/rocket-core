// =============================================================================
// test/mocks/Adafruit_ADXL375.h — driver mock. Signatures match the real
// Adafruit_ADXL375 library (constructor takes a sensor ID; begin() takes the
// I2C address; data rate enum is shared with the ADXL343 driver).
// =============================================================================
#pragma once
#include <cstdint>

#include "Adafruit_Sensor.h"
#include "mock_state.h"

typedef enum {
  ADXL343_DATARATE_200_HZ = 11,
} adxl343_dataRate_t;

#define ADXL375_DEFAULT_ADDRESS 0x53

class Adafruit_ADXL375 {
public:
  explicit Adafruit_ADXL375(int32_t sensorID) { (void)sensorID; }

  bool begin(uint8_t addr = ADXL375_DEFAULT_ADDRESS) {
    return g_mock.accelPresent && g_mock.i2cPresent.count(addr) != 0;
  }
  void setDataRate(adxl343_dataRate_t) {}
  bool getEvent(sensors_event_t *e) {
    if (!g_mock.accelOk) {
      return false;
    }
    e->acceleration.x = g_mock.acc[0];
    e->acceleration.y = g_mock.acc[1];
    e->acceleration.z = g_mock.acc[2];
    return true;
  }
};
