// =============================================================================
// test/firmware_smoke.cpp — runs the UNMODIFIED sketch on a host machine
//
// Includes lurs_flight_computer.ino verbatim, compiled against the mock
// Arduino environment in test/mocks/. The harness drives setup()/loop() with
// the physics simulator and then asserts on the actual CSV file the sketch
// "wrote to SD".
//
// Modes (one per process — the sketch uses globals, as Arduino sketches do):
//   ok         full simulated flight; checks the produced log end to end
//   alt_addr   BMP388 on the secondary address 0x76 — fallback must work
//   sd_seq     FLIGHT00/01 already on card — must pick FLIGHT02.CSV
//   sd_fail    SD card dead     -> expect fatal code 2
//   baro_fail  BMP388 missing   -> expect fatal code 3
//   accel_fail ADXL375 missing  -> expect fatal code 4
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// Mock environment (found first via -I mocks). HOST_TEST is set by the build.
#include "Arduino.h"

#include "../firmware/lurs_flight_computer/lurs_flight_computer.ino"

#include "sim_physics.h"

static int g_fatal = -1;
void host_fatal(uint8_t code) { g_fatal = (int)code; }

static int g_fails = 0;
#define CHECK(cond, ...)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      g_fails++;                                                               \
      printf("FAIL %s:%d: ", __FILE__, __LINE__);                              \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
    }                                                                          \
  } while (0)

// Inverse of the sketch's ISA conversion: altitude (m ASL) -> pressure (Pa)
static double altToPressPa(double h) {
  return 101325.0 * pow(1.0 - h / 44330.0, 1.0 / 0.190295);
}

// Mount the simulated |accel| on a fixed tilted axis — the sketch must be
// orientation-independent because it works on the vector magnitude.
static void applySample(const SimSample &s) {
  g_mock.baroOk = s.baroValid;
  g_mock.pressPa = altToPressPa(s.baroAltAsl);
  g_mock.tempC = 15.0 - 0.0065 * s.baroAltAsl;
  g_mock.accelOk = s.accelValid;
  g_mock.acc[0] = 0.30f * s.accelMag;
  g_mock.acc[1] = 0.50f * s.accelMag;
  g_mock.acc[2] = 0.8124f * s.accelMag; // (0.30, 0.50, 0.8124) is unit length
}

static void presentAll() {
  g_mock.i2cPresent = {0x77, 0x53};
  g_mock.baroPresent = true;
  g_mock.accelPresent = true;
  g_mock.sdBeginOk = true;
  g_mock.sdOpenOk = true;
}

// ---------------------------------------------------------------------------
static int countDataRows(const std::string &log, std::string *lastRow) {
  int rows = 0;
  size_t pos = 0;
  while (pos < log.size()) {
    size_t nl = log.find('\n', pos);
    if (nl == std::string::npos) {
      nl = log.size();
    }
    if (nl > pos && log[pos] >= '0' && log[pos] <= '9') {
      rows++;
      if (lastRow != nullptr) {
        lastRow->assign(log, pos, nl - pos);
      }
    }
    pos = nl + 1;
  }
  return rows;
}

static int commasIn(const std::string &s) {
  int n = 0;
  for (char c : s) {
    if (c == ',') {
      n++;
    }
  }
  return n;
}

// ---------------------------------------------------------------------------
static void modeOk() {
  presentAll();
  SimParams p;
  FlightSim sim(p);
  g_mock.ms = 500;
  applySample(sim.step(0.0f, 0.01f)); // boot-time sensor values
  setup();
  CHECK(g_fatal == -1, "unexpected fatal %d during setup", g_fatal);
  CHECK(g_mock.sdOpenedPath == "FLIGHT00.CSV", "opened '%s', expected FLIGHT00.CSV",
        g_mock.sdOpenedPath.c_str());

  for (int i = 1; i <= 11000; i++) { // 110 s @ 100 Hz, bounded
    g_mock.ms += 10;
    applySample(sim.step((float)i * 0.01f, 0.01f));
    loop();
  }

  const std::string &log = g_mock.sdFileContent;
  CHECK(log.find("# rocket1-logger") != std::string::npos, "version header missing");
  CHECK(log.find("t_ms,state,press_pa,temp_c,alt_asl_m,alt_agl_m,vspeed_ms,"
                 "ax_ms2,ay_ms2,az_ms2,accmag_ms2") != std::string::npos,
        "CSV header missing");
  CHECK(log.find("i2c devices: 0x53 0x77") != std::string::npos,
        "i2c scan report missing/wrong");
  CHECK(log.find("baro@0x77 accel@0x53") != std::string::npos,
        "address report missing/wrong");

  const size_t iArm = log.find("ARMED");
  const size_t iLau = log.find("LAUNCH");
  const size_t iBur = log.find("BURNOUT");
  const size_t iApo = log.find("APOGEE");
  const size_t iLnd = log.find("LANDED");
  CHECK(iArm != std::string::npos, "ARMED event missing");
  CHECK(iLau != std::string::npos, "LAUNCH event missing");
  CHECK(iBur != std::string::npos, "BURNOUT event missing");
  CHECK(iApo != std::string::npos, "APOGEE event missing");
  CHECK(iLnd != std::string::npos, "LANDED event missing");
  CHECK(iArm < iLau && iLau < iBur && iBur < iApo && iApo < iLnd,
        "events out of order");

  CHECK(log.find("nan") == std::string::npos, "NaN leaked into the log");

  std::string last;
  const int rows = countDataRows(log, &last);
  CHECK(rows > 5500, "only %d data rows (expected > 5500)", rows);
  CHECK(commasIn(last) == 10, "last row has %d commas, expected 10: '%s'",
        commasIn(last), last.c_str());
  CHECK(last.find(",LND,") != std::string::npos,
        "last row not in LANDED state: '%s'", last.c_str());
  CHECK(g_mock.sdFlushes > 50, "only %d flushes — flush cadence broken",
        g_mock.sdFlushes);

  printf("smoke ok: %d rows, %d flushes, log %zu bytes\n", rows,
         g_mock.sdFlushes, log.size());

  // Dump the captured "SD card" log — used to test tools/analyze_flight.py
  // and as a sample of what a real FLIGHTnn.CSV looks like.
  FILE *f = fopen("out/FLIGHT00.CSV", "w");
  if (f != nullptr) {
    fwrite(log.data(), 1, log.size(), f);
    fclose(f);
  }
}

static void modeAltAddr() {
  presentAll();
  g_mock.i2cPresent = {0x76, 0x53}; // BMP388 strapped to secondary address
  SimParams p;
  FlightSim sim(p);
  g_mock.ms = 500;
  applySample(sim.step(0.0f, 0.01f));
  setup();
  CHECK(g_fatal == -1, "fallback address boot failed (fatal %d)", g_fatal);
  CHECK(g_mock.sdFileContent.find("baro@0x76") != std::string::npos,
        "secondary baro address not reported");
  printf("smoke alt_addr: ok\n");
}

static void modeSdSeq() {
  presentAll();
  g_mock.sdExisting = {"FLIGHT00.CSV", "FLIGHT01.CSV"};
  SimParams p;
  FlightSim sim(p);
  g_mock.ms = 500;
  applySample(sim.step(0.0f, 0.01f));
  setup();
  CHECK(g_fatal == -1, "unexpected fatal %d", g_fatal);
  CHECK(g_mock.sdOpenedPath == "FLIGHT02.CSV", "opened '%s', expected FLIGHT02.CSV",
        g_mock.sdOpenedPath.c_str());
  printf("smoke sd_seq: ok\n");
}

static void modeSdFail() {
  presentAll();
  g_mock.sdBeginOk = false;
  g_mock.ms = 500;
  setup();
  CHECK(g_fatal == 2, "expected fatal 2 (SD), got %d", g_fatal);
  printf("smoke sd_fail: ok\n");
}

static void modeBaroFail() {
  presentAll();
  g_mock.i2cPresent = {0x53}; // accel only — baro absent on both addresses
  g_mock.ms = 500;
  setup();
  CHECK(g_fatal == 3, "expected fatal 3 (BMP388), got %d", g_fatal);
  printf("smoke baro_fail: ok\n");
}

static void modeAccelFail() {
  presentAll();
  g_mock.i2cPresent = {0x77}; // baro only — accel absent on both addresses
  g_mock.ms = 500;
  setup();
  CHECK(g_fatal == 4, "expected fatal 4 (ADXL375), got %d", g_fatal);
  printf("smoke accel_fail: ok\n");
}

// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: %s ok|alt_addr|sd_seq|sd_fail|baro_fail|accel_fail\n",
           argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "ok") == 0) {
    modeOk();
  } else if (strcmp(argv[1], "alt_addr") == 0) {
    modeAltAddr();
  } else if (strcmp(argv[1], "sd_seq") == 0) {
    modeSdSeq();
  } else if (strcmp(argv[1], "sd_fail") == 0) {
    modeSdFail();
  } else if (strcmp(argv[1], "baro_fail") == 0) {
    modeBaroFail();
  } else if (strcmp(argv[1], "accel_fail") == 0) {
    modeAccelFail();
  } else {
    printf("unknown mode '%s'\n", argv[1]);
    return 2;
  }
  return (g_fails == 0) ? 0 : 1;
}
