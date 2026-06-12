// =============================================================================
// LURS Rocket 1 — flight data logger
// lurs_flight_computer.ino
//
// Hardware : Arduino Nano ESP32 (or Nano 33 IoT — select the right board in
//            the IDE), BMP388 barometer + ADXL375 high-g accelerometer on
//            I2C, microSD module on SPI. See config.h and docs/WIRING.md.
//
// Function : standalone flight data logger. Samples both sensors at 100 Hz,
//            runs the flight state machine (flight_logic.h), and streams
//            everything to FLIGHTnn.CSV on the SD card. No outputs are
//            controlled — this rocket recovers via motor ejection; the
//            logger is instrumentation only.
//
// Style    : no dynamic allocation (no String, no malloc), no recursion,
//            bounded loops, fixed-size buffers — per the society's
//            safety-critical coding standards.
//
// LED codes (built-in LED):
//   fast flicker (10 Hz)     CALIBRATING — keep the rocket still
//   short blip every 1 s     PAD / armed — ready to fly
//   solid on                 in flight (boost/coast/descent)
//   double-blip every 2 s    LANDED — safe to power off
//   repeating N blinks, long pause = FATAL, do not fly:
//     2 = SD card failed   3 = BMP388 failed   4 = ADXL375 failed
// =============================================================================

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include <Adafruit_ADXL375.h>
#include <Adafruit_BMP3XX.h>
#include <Adafruit_Sensor.h>

#include "config.h"
#include "flight_logic.h"

// ESP32 SD paths need a leading '/', SAMD does not mind either way.
#if defined(ARDUINO_ARCH_ESP32)
#define SD_PATH_PREFIX "/"
#else
#define SD_PATH_PREFIX ""
#endif

// ---------------------------------------------------------------------------
// Globals (fixed allocation only)
// ---------------------------------------------------------------------------
static Adafruit_BMP3XX g_bmp;
static Adafruit_ADXL375 g_adxl(0x375); // arg = arbitrary sensor ID
static FlightLogic g_logic;
static File g_logFile;

static char g_logBuf[LOG_BUFFER_BYTES];
static size_t g_logLen = 0;

static uint32_t g_nextTickMs = 0;
static uint32_t g_lastFlushMs = 0;
static uint32_t g_lastLandedRowMs = 0;
static uint16_t g_overruns = 0;
static FlightState g_prevState = FlightState::CALIBRATING;

static uint8_t g_baroAddr = 0;
static uint8_t g_accelAddr = 0;
static char g_logPath[24] = {0};

// ---------------------------------------------------------------------------
// Fatal error handling — blink forever, never fly with a dead logger.
// (HOST_TEST hook lets the test harness intercept instead of looping.)
// ---------------------------------------------------------------------------
#ifdef HOST_TEST
void host_fatal(uint8_t code); // provided by the test harness
#endif

#ifndef HOST_TEST
static void blinkCode(uint8_t blinks) {
  for (uint8_t i = 0; i < blinks; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
  delay(1200);
}
#endif

static void fatalError(uint8_t code, const char *msg) {
  Serial.print("FATAL: ");
  Serial.println(msg);
#ifdef HOST_TEST
  host_fatal(code);
#else
  for (;;) {
    blinkCode(code);
  }
#endif
}

// ---------------------------------------------------------------------------
// Portable float formatting. SAMD's newlib-nano snprintf does not support
// %f, so floats are formatted manually. Bounded, no heap.
// ---------------------------------------------------------------------------
static size_t fmtFloat(char *out, size_t cap, float v, uint8_t dp) {
  if (cap == 0) {
    return 0;
  }
  if (!isfinite(v)) {
    return (size_t)snprintf(out, cap, "nan");
  }
  if (dp > 3) {
    dp = 3;
  }
  static const int32_t SCALE[4] = {1, 10, 100, 1000};
  const int32_t scale = SCALE[dp];
  // Keep |v| * scale comfortably inside int32 (long is 32-bit on the MCU).
  const float vmax = 2.0e9f / (float)scale;
  if (v > vmax) {
    v = vmax;
  }
  if (v < -vmax) {
    v = -vmax;
  }
  const bool neg = (v < 0.0f);
  const float av = neg ? -v : v;
  const int32_t whole = (int32_t)(av * (float)scale + 0.5f);
  const int32_t ip = whole / scale;
  int fp = (int)(whole % scale);
  if (fp < 0) {
    fp = 0; // unreachable (whole >= 0); keeps the bound obvious
  }
  if (fp > 999) {
    fp = 999; // unreachable (scale <= 1000); keeps the bound obvious
  }
  if (dp == 0) {
    return (size_t)snprintf(out, cap, "%s%ld", neg ? "-" : "", (long)ip);
  }
  return (size_t)snprintf(out, cap, "%s%ld.%0*d", neg ? "-" : "", (long)ip,
                          (int)dp, fp);
}

// ---------------------------------------------------------------------------
// Buffered SD logging
// ---------------------------------------------------------------------------
static void sdWriteBuf() {
  if (g_logLen > 0 && g_logFile) {
    g_logFile.write(reinterpret_cast<const uint8_t *>(g_logBuf), g_logLen);
    g_logLen = 0;
  }
}

static void bufAppend(const char *s) {
  while (*s != '\0') {
    if (g_logLen >= (LOG_BUFFER_BYTES - 1)) {
      sdWriteBuf(); // buffer full — push to card now
    }
    g_logBuf[g_logLen++] = *s++;
  }
}

static void bufAppendFloat(float v, uint8_t dp) {
  char tmp[20];
  fmtFloat(tmp, sizeof(tmp), v, dp);
  bufAppend(tmp);
}

static void bufAppendU32(uint32_t v) {
  char tmp[14];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)v);
  bufAppend(tmp);
}

// Comment / event line: written through and flushed immediately so launch,
// apogee and landing markers survive even if power is lost moments later.
static void logComment(const char *msg) {
  bufAppend("# ");
  bufAppend(msg);
  bufAppend("\n");
  sdWriteBuf();
  if (g_logFile) {
    g_logFile.flush();
  }
  Serial.print("# ");
  Serial.println(msg);
}

// ---------------------------------------------------------------------------
// Sensor / SD bring-up (retries + automatic address fallback)
// ---------------------------------------------------------------------------
static void i2cScanReport() {
  char line[64];
  char *p = line;
  size_t left = sizeof(line);
  size_t n = (size_t)snprintf(p, left, "i2c devices:");
  p += n;
  left -= n;
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) { // bounded
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (found < 6 && left > 6) {
        n = (size_t)snprintf(p, left, " 0x%02X", addr);
        p += n;
        left -= n;
      }
      found++;
    }
  }
  if (found == 0) {
    snprintf(p, left, " none");
  }
  logComment(line);
}

static bool initBaro() {
  const uint8_t addrs[2] = {BMP388_ADDR_PRIMARY, BMP388_ADDR_SECONDARY};
  for (uint8_t a = 0; a < 2; a++) {
    for (uint8_t attempt = 0; attempt < 3; attempt++) { // bounded retries
      if (g_bmp.begin_I2C(addrs[a], &Wire)) {
        g_baroAddr = addrs[a];
        g_bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
        g_bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
        g_bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
        g_bmp.setOutputDataRate(BMP3_ODR_50_HZ);
        (void)g_bmp.performReading(); // prime first conversion
        return true;
      }
      delay(100);
    }
  }
  return false;
}

static bool initAccel() {
  const uint8_t addrs[2] = {ADXL375_ADDR_PRIMARY, ADXL375_ADDR_SECONDARY};
  for (uint8_t a = 0; a < 2; a++) {
    for (uint8_t attempt = 0; attempt < 3; attempt++) { // bounded retries
      if (g_adxl.begin(addrs[a])) {
        g_accelAddr = addrs[a];
        g_adxl.setDataRate(ADXL343_DATARATE_200_HZ);
        return true;
      }
      delay(100);
    }
  }
  return false;
}

static bool initSdAndOpenFile() {
  bool ok = false;
  for (uint8_t attempt = 0; attempt < 3; attempt++) { // bounded retries
    if (SD.begin(PIN_SD_CS)) {
      ok = true;
      break;
    }
    delay(200);
  }
  if (!ok) {
    return false;
  }
  // First unused FLIGHTnn.CSV, new file every boot.
  for (uint8_t i = 0; i <= 99; i++) { // bounded
    snprintf(g_logPath, sizeof(g_logPath), SD_PATH_PREFIX "FLIGHT%02u.CSV",
             (unsigned)i);
    if (!SD.exists(g_logPath)) {
      break;
    }
  }
  g_logFile = SD.open(g_logPath, FILE_WRITE);
  return (bool)g_logFile;
}

// ---------------------------------------------------------------------------
// Altitude from pressure (ISA model). Absolute value is only approximate —
// the state machine works on altitude *above the pad baseline*, which
// cancels the reference-pressure error.
// ---------------------------------------------------------------------------
static float pressureToAltitudeM(float pressPa) {
  if (!isfinite(pressPa) || pressPa < 1000.0f) {
    return NAN;
  }
  return 44330.0f * (1.0f - powf(pressPa / 101325.0f, 0.190295f));
}

// ---------------------------------------------------------------------------
// CSV output
// ---------------------------------------------------------------------------
static void writeCsvHeader(float bootTempC, float bootPressPa) {
  char line[96];
  logComment(FIRMWARE_VERSION);
  snprintf(line, sizeof(line), "log file %s, loop %u Hz", g_logPath,
           (unsigned)(1000 / LOOP_PERIOD_MS));
  logComment(line);
  {
    char t[16], p[16];
    fmtFloat(t, sizeof(t), bootTempC, 1);
    fmtFloat(p, sizeof(p), bootPressPa, 1);
    snprintf(line, sizeof(line), "boot temp_c=%s press_pa=%s baro@0x%02X accel@0x%02X",
             t, p, g_baroAddr, g_accelAddr);
    logComment(line);
  }
  bufAppend("t_ms,state,press_pa,temp_c,alt_asl_m,alt_agl_m,vspeed_ms,"
            "ax_ms2,ay_ms2,az_ms2,accmag_ms2\n");
  sdWriteBuf();
  g_logFile.flush();
}

static void appendCsvRow(uint32_t nowMs, FlightState st, float pressPa,
                         float tempC, float altAsl, float ax, float ay,
                         float az, float mag) {
  bufAppendU32(nowMs);
  bufAppend(",");
  bufAppend(flightStateName(st));
  bufAppend(",");
  bufAppendFloat(pressPa, 1);
  bufAppend(",");
  bufAppendFloat(tempC, 1);
  bufAppend(",");
  bufAppendFloat(altAsl, 2);
  bufAppend(",");
  bufAppendFloat(g_logic.altitudeAgl(), 2);
  bufAppend(",");
  bufAppendFloat(g_logic.verticalSpeed(), 2);
  bufAppend(",");
  bufAppendFloat(ax, 2);
  bufAppend(",");
  bufAppendFloat(ay, 2);
  bufAppend(",");
  bufAppendFloat(az, 2);
  bufAppend(",");
  bufAppendFloat(mag, 2);
  bufAppend("\n");
}

static void onStateChange(FlightState from, FlightState to, uint32_t nowMs) {
  char line[112];
  char a[16], b[16];
  switch (to) {
  case FlightState::PAD:
    fmtFloat(a, sizeof(a), g_logic.baselineAsl(), 2);
    snprintf(line, sizeof(line), "EVENT t=%lu ARMED baseline_asl_m=%s",
             (unsigned long)nowMs, a);
    break;
  case FlightState::BOOST:
    snprintf(line, sizeof(line), "EVENT t=%lu LAUNCH", (unsigned long)nowMs);
    break;
  case FlightState::COAST:
    snprintf(line, sizeof(line), "EVENT t=%lu BURNOUT", (unsigned long)nowMs);
    break;
  case FlightState::DESCENT:
    fmtFloat(a, sizeof(a), g_logic.maxAltitudeAgl(), 2);
    snprintf(line, sizeof(line), "EVENT t=%lu APOGEE agl_m=%s t_apogee=%lu",
             (unsigned long)nowMs, a, (unsigned long)g_logic.apogeeTimeMs());
    break;
  case FlightState::LANDED:
    fmtFloat(a, sizeof(a), g_logic.maxAltitudeAgl(), 2);
    fmtFloat(b, sizeof(b), g_logic.altitudeAgl(), 2);
    snprintf(line, sizeof(line),
             "EVENT t=%lu LANDED max_agl_m=%s final_agl_m=%s overruns=%u",
             (unsigned long)nowMs, a, b, (unsigned)g_overruns);
    break;
  default:
    snprintf(line, sizeof(line), "EVENT t=%lu %s->%s", (unsigned long)nowMs,
             flightStateName(from), flightStateName(to));
    break;
  }
  logComment(line); // writes through + flushes
}

// ---------------------------------------------------------------------------
// Status LED
// ---------------------------------------------------------------------------
static void updateStatusLed(uint32_t nowMs, FlightState st) {
  bool on = false;
  switch (st) {
  case FlightState::CALIBRATING:
    on = ((nowMs / 50) & 1u) != 0u; // 10 Hz flicker
    break;
  case FlightState::PAD:
    on = (nowMs % 1000) < 100; // heartbeat blip
    break;
  case FlightState::BOOST:
  case FlightState::COAST:
  case FlightState::DESCENT:
    on = true; // solid
    break;
  case FlightState::LANDED: {
    const uint32_t ph = nowMs % 2000;
    on = (ph < 100) || (ph >= 250 && ph < 350); // double blip
    break;
  }
  }
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(SERIAL_BAUD);
  const uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < SERIAL_WAIT_MS) {
    // bounded wait — must boot headless on the pad
  }
  Serial.println();
  Serial.println(FIRMWARE_VERSION);

  Wire.begin();
  Wire.setClock(400000UL);

  if (!initBaro()) {
    fatalError(3, "BMP388 not responding (tried 0x77/0x76)");
    return; // reached only under HOST_TEST
  }
  if (!initAccel()) {
    fatalError(4, "ADXL375 not responding (tried 0x53/0x1D)");
    return;
  }
  if (!initSdAndOpenFile()) {
    fatalError(2, "SD card init/open failed (FAT32? CS pin? inserted?)");
    return;
  }

  // Boot diagnostics into the log file itself, so the launch-day person can
  // verify the stack with nothing but a card reader.
  float bootTemp = NAN;
  float bootPress = NAN;
  if (g_bmp.performReading()) {
    bootTemp = (float)g_bmp.temperature;
    bootPress = (float)g_bmp.pressure;
  }
  writeCsvHeader(bootTemp, bootPress);
  i2cScanReport();
  logComment("calibrating ground baseline - keep rocket still");

  g_logic.begin(millis());
  g_prevState = g_logic.state();
  g_nextTickMs = millis();
  g_lastFlushMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if ((int32_t)(now - g_nextTickMs) < 0) {
    return; // not time for the next tick yet
  }
  g_nextTickMs += LOOP_PERIOD_MS;
  if ((int32_t)(now - g_nextTickMs) > 200) {
    // We fell badly behind (e.g. a slow SD flush). Resync rather than
    // burst-running stale ticks.
    g_nextTickMs = now + LOOP_PERIOD_MS;
    g_overruns++;
  }

  // --- sample sensors -------------------------------------------------------
  const bool baroOk = g_bmp.performReading();
  const float pressPa = baroOk ? (float)g_bmp.pressure : NAN;
  const float tempC = baroOk ? (float)g_bmp.temperature : NAN;
  const float altAsl = baroOk ? pressureToAltitudeM(pressPa) : NAN;

  sensors_event_t ev;
  const bool accOk = g_adxl.getEvent(&ev);
  const float ax = accOk ? ev.acceleration.x : NAN;
  const float ay = accOk ? ev.acceleration.y : NAN;
  const float az = accOk ? ev.acceleration.z : NAN;
  const float mag =
      accOk ? sqrtf(ax * ax + ay * ay + az * az) : NAN;

  // --- flight logic ----------------------------------------------------------
  g_logic.update(now, baroOk && isfinite(altAsl), altAsl,
                 accOk && isfinite(mag), mag);

  const FlightState st = g_logic.state();
  if (st != g_prevState) {
    onStateChange(g_prevState, st, now);
    g_prevState = st;
  }

  // --- log -------------------------------------------------------------------
  bool writeRow = true;
  if (st == FlightState::LANDED) {
    if ((now - g_lastLandedRowMs) < LANDED_ROW_PERIOD_MS) {
      writeRow = false;
    } else {
      g_lastLandedRowMs = now;
    }
  }
  if (writeRow) {
    appendCsvRow(now, st, pressPa, tempC, altAsl, ax, ay, az, mag);
  }

  if (g_logLen >= LOG_WRITE_THRESHOLD) {
    sdWriteBuf();
  }
  if ((now - g_lastFlushMs) >= LOG_FLUSH_MS) {
    sdWriteBuf();
    if (g_logFile) {
      g_logFile.flush();
    }
    g_lastFlushMs = now;
  }

  updateStatusLed(now, st);
}
