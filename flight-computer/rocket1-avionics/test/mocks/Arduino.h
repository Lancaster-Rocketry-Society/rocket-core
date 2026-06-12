// =============================================================================
// test/mocks/Arduino.h — minimal Arduino core mock for host builds
// Provides exactly what lurs_flight_computer.ino uses, driven by g_mock.
// =============================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "mock_state.h"

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

inline uint32_t millis() { return (uint32_t)g_mock.ms; }
inline void delay(uint32_t ms) { g_mock.ms += ms; } // advances mock time
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

struct MockSerial {
  void begin(unsigned long) {}
  explicit operator bool() const { return true; }
  void print(const char *s) { fputs(s, stdout); }
  void println(const char *s) {
    fputs(s, stdout);
    fputc('\n', stdout);
  }
  void println() { fputc('\n', stdout); }
};

extern MockSerial Serial;
