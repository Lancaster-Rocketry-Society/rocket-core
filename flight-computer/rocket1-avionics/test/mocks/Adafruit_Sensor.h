// =============================================================================
// test/mocks/Adafruit_Sensor.h — just the event struct shape the sketch uses.
// =============================================================================
#pragma once
struct sensors_vec_t {
  float x;
  float y;
  float z;
};
struct sensors_event_t {
  sensors_vec_t acceleration;
};
