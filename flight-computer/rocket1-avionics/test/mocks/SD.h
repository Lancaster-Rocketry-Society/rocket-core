// =============================================================================
// test/mocks/SD.h — SD mock. Captures every byte the sketch writes into
// g_mock.sdFileContent so tests can assert on the actual log file produced.
// =============================================================================
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "mock_state.h"

#define FILE_WRITE 1

class File {
public:
  bool open_ = false;
  size_t write(const uint8_t *b, size_t n) {
    if (!open_) {
      return 0;
    }
    g_mock.sdFileContent.append(reinterpret_cast<const char *>(b), n);
    return n;
  }
  void flush() {
    if (open_) {
      g_mock.sdFlushes++;
    }
  }
  explicit operator bool() const { return open_; }
};

class SDClass {
public:
  bool begin(uint8_t) { return g_mock.sdBeginOk; }
  bool exists(const char *p) { return g_mock.sdExisting.count(p) != 0; }
  File open(const char *p, int) {
    File f;
    f.open_ = g_mock.sdOpenOk;
    if (f.open_) {
      g_mock.sdOpenedPath = p;
    }
    return f;
  }
};

extern SDClass SD;
