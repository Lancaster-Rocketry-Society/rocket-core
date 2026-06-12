// =============================================================================
// LURS Rocket 1 — flight data logger
// flight_logic.h — portable flight state machine
//
// Pure C++: no Arduino dependencies, no heap allocation, no recursion, all
// loops bounded. The exact code in this file is compiled and tested on a host
// machine against simulated flights (see test/) — what flies is what was
// tested.
//
// State machine:
//   CALIBRATING -> PAD -> BOOST -> COAST -> DESCENT -> LANDED
//
// Detection summary:
//   Launch  : |accel| > launchAccelMs2 sustained launchAccelHoldMs
//             OR altitude AGL > launchAltM sustained launchAltHoldMs (backup,
//             covers a weak/slow motor or a dead accelerometer)
//   Burnout : |accel| < burnoutAccelMs2 after minBoostMs
//   Apogee  : filtered altitude drops apogeeDropM below the running maximum
//   Landed  : altitude span over a rolling 12 s window < landedBandM
//
// The pad baseline tracks slow barometric drift (weather) while on the pad,
// and freezes at launch.
// =============================================================================
#pragma once
#include <math.h>
#include <stdint.h>

enum class FlightState : uint8_t {
  CALIBRATING = 0, // measuring ground-level baseline, do not move the rocket
  PAD,             // armed and waiting for launch
  BOOST,           // motor burning
  COAST,           // burnout to apogee
  DESCENT,         // past apogee
  LANDED           // on the ground, logging at reduced rate
};

inline const char *flightStateName(FlightState s) {
  switch (s) {
  case FlightState::CALIBRATING:
    return "CAL";
  case FlightState::PAD:
    return "PAD";
  case FlightState::BOOST:
    return "BST";
  case FlightState::COAST:
    return "CST";
  case FlightState::DESCENT:
    return "DSC";
  case FlightState::LANDED:
    return "LND";
  }
  return "UNK";
}

struct FlightTunables {
  uint32_t calibrationMs = 5000;    // min time spent measuring pad baseline
  uint32_t calibrationSamples = 100; // min valid baro samples for baseline

  float launchAccelMs2 = 35.0f;     // ~3.6 g — primary launch trigger
  uint32_t launchAccelHoldMs = 100; // must be sustained this long
  float launchAltM = 20.0f;         // backup trigger: altitude above pad
  uint32_t launchAltHoldMs = 300;   // must be sustained this long

  uint32_t minBoostMs = 300;     // ignore burnout checks before this
  float burnoutAccelMs2 = 20.0f; // |accel| below this after boost => COAST

  float apogeeDropM = 3.0f; // fall this far below max altitude => DESCENT
  uint32_t minApogeeAfterLaunchMs = 1000; // apogee can't be before this

  float landedBandM = 2.5f;            // alt span over 12 s => LANDED
  uint32_t landedFailsafeMs = 30000;   // allow LANDED from COAST after this
                                       // (covers a missed apogee detection)

  float altLpAlpha = 0.25f;        // altitude low-pass (per 100 Hz sample)
  float vSpeedAlpha = 0.15f;       // vertical-speed smoothing
  float calibAlpha = 0.05f;        // baseline EMA during calibration
  float padBaselineAlpha = 0.0008f; // slow drift tracking on pad (~12 s tau)
};

class FlightLogic {
public:
  // Call once at boot.
  void begin(uint32_t nowMs) { beginWith(nowMs, FlightTunables()); }

  void beginWith(uint32_t nowMs, const FlightTunables &t) {
    tun_ = t;
    st_ = FlightState::CALIBRATING;
    startMs_ = nowMs;
    lastMs_ = nowMs;
    altInit_ = false;
    altF_ = 0.0f;
    prevAltF_ = 0.0f;
    vSpd_ = 0.0f;
    baseline_ = 0.0f;
    baselineInit_ = false;
    calibCount_ = 0;
    lastBaroMs_ = 0;
    haveBaro_ = false;
    accelOver_ = false;
    altOver_ = false;
    accelOverSince_ = 0;
    altOverSince_ = 0;
    launchMs_ = 0;
    apogeeMs_ = 0;
    landedMs_ = 0;
    maxAgl_ = 0.0f;
    ringCount_ = 0;
    ringHead_ = 0;
    lastRingMs_ = 0;
    for (int i = 0; i < RING_N; i++) {
      ring_[i] = 0.0f;
    }
  }

  // Call every loop tick (~100 Hz). Pass validity flags honestly: a sensor
  // that failed to read this tick must come in as valid=false. The logic is
  // designed to keep working through dropouts of either sensor.
  //   altAslM      : barometric altitude, metres above sea level (ISA)
  //   accelMagMs2  : |acceleration| vector magnitude, m/s^2 (~9.81 at rest)
  void update(uint32_t nowMs, bool baroValid, float altAslM, bool accelValid,
              float accelMagMs2) {
    uint32_t dtMs = nowMs - lastMs_;
    if (dtMs < 1) {
      dtMs = 1;
    }
    if (dtMs > 1000) {
      dtMs = 1000;
    }
    lastMs_ = nowMs;

    const bool baroGood = baroValid && isfinite(altAslM);
    const bool accelGood = accelValid && isfinite(accelMagMs2);

    if (baroGood) {
      lastBaroMs_ = nowMs;
      haveBaro_ = true;
      if (!altInit_) {
        altF_ = altAslM;
        prevAltF_ = altAslM;
        altInit_ = true;
      } else {
        altF_ += tun_.altLpAlpha * (altAslM - altF_);
        const float instV = (altF_ - prevAltF_) * (1000.0f / (float)dtMs);
        prevAltF_ = altF_;
        vSpd_ += tun_.vSpeedAlpha * (instV - vSpd_);
      }
    }

    const bool baroFresh = haveBaro_ && (nowMs - lastBaroMs_) <= 600;

    switch (st_) {
    case FlightState::CALIBRATING:
      if (baroGood) {
        if (!baselineInit_) {
          baseline_ = altF_;
          baselineInit_ = true;
        } else {
          baseline_ += tun_.calibAlpha * (altF_ - baseline_);
        }
        calibCount_++;
      }
      if (calibCount_ >= tun_.calibrationSamples &&
          (nowMs - startMs_) >= tun_.calibrationMs) {
        st_ = FlightState::PAD;
      }
      break;

    case FlightState::PAD: {
      // Track slow weather drift so hours on the pad can't creep toward the
      // altitude launch trigger.
      if (baroGood) {
        baseline_ += tun_.padBaselineAlpha * (altF_ - baseline_);
      }
      const float agl = altitudeAgl();

      // Primary trigger: sustained high acceleration.
      if (accelGood && accelMagMs2 > tun_.launchAccelMs2) {
        if (!accelOver_) {
          accelOver_ = true;
          accelOverSince_ = nowMs;
        } else if ((nowMs - accelOverSince_) >= tun_.launchAccelHoldMs) {
          enterBoost(nowMs);
          break;
        }
      } else {
        accelOver_ = false;
      }

      // Backup trigger: sustained altitude gain (weak motor / dead accel).
      if (baroFresh && agl > tun_.launchAltM) {
        if (!altOver_) {
          altOver_ = true;
          altOverSince_ = nowMs;
        } else if ((nowMs - altOverSince_) >= tun_.launchAltHoldMs) {
          enterBoost(nowMs);
          break;
        }
      } else {
        altOver_ = false;
      }
      break;
    }

    case FlightState::BOOST:
      trackMax(nowMs, baroFresh);
      if ((nowMs - launchMs_) >= tun_.minBoostMs && accelGood &&
          accelMagMs2 < tun_.burnoutAccelMs2) {
        st_ = FlightState::COAST;
        break;
      }
      checkApogee(nowMs, baroFresh); // failsafe: apogee straight from boost
      break;

    case FlightState::COAST:
      trackMax(nowMs, baroFresh);
      checkApogee(nowMs, baroFresh);
      if (st_ == FlightState::COAST) {
        // Failsafe: if apogee was never seen (e.g. long baro outage) but the
        // rocket has clearly been still on the ground for 12 s, land anyway.
        pushRing(nowMs, baroFresh);
        if ((nowMs - launchMs_) >= tun_.landedFailsafeMs && ringStable()) {
          enterLanded(nowMs);
        }
      }
      break;

    case FlightState::DESCENT:
      pushRing(nowMs, baroFresh);
      if (ringStable()) {
        enterLanded(nowMs);
      }
      break;

    case FlightState::LANDED:
      break;
    }
  }

  FlightState state() const { return st_; }
  bool armed() const { return st_ != FlightState::CALIBRATING; }
  float altitudeAgl() const {
    if (!altInit_ || !baselineInit_) {
      return 0.0f;
    }
    return altF_ - baseline_;
  }
  float verticalSpeed() const { return vSpd_; }
  float baselineAsl() const { return baselineInit_ ? baseline_ : 0.0f; }
  float maxAltitudeAgl() const { return maxAgl_; }
  uint32_t launchTimeMs() const { return launchMs_; }
  uint32_t apogeeTimeMs() const { return apogeeMs_; }
  uint32_t landedTimeMs() const { return landedMs_; }

private:
  void enterBoost(uint32_t nowMs) {
    st_ = FlightState::BOOST;
    launchMs_ = nowMs;
    apogeeMs_ = nowMs;
    maxAgl_ = altitudeAgl();
  }

  void enterLanded(uint32_t nowMs) {
    st_ = FlightState::LANDED;
    landedMs_ = nowMs;
  }

  void trackMax(uint32_t nowMs, bool baroFresh) {
    if (!baroFresh) {
      return;
    }
    const float agl = altitudeAgl();
    if (agl > maxAgl_) {
      maxAgl_ = agl;
      apogeeMs_ = nowMs;
    }
  }

  void checkApogee(uint32_t nowMs, bool baroFresh) {
    if (!baroFresh) {
      return;
    }
    if ((nowMs - launchMs_) < tun_.minApogeeAfterLaunchMs) {
      return;
    }
    if (altitudeAgl() < (maxAgl_ - tun_.apogeeDropM)) {
      st_ = FlightState::DESCENT;
    }
  }

  // Rolling 12 s altitude window, sampled at 2 Hz. Only fresh, valid baro
  // samples are pushed, so a dead barometer can never fake a landing.
  void pushRing(uint32_t nowMs, bool baroFresh) {
    if (!baroFresh) {
      return;
    }
    if ((nowMs - lastRingMs_) < 500 && ringCount_ > 0) {
      return;
    }
    lastRingMs_ = nowMs;
    ring_[ringHead_] = altitudeAgl();
    ringHead_ = (ringHead_ + 1) % RING_N;
    if (ringCount_ < RING_N) {
      ringCount_++;
    }
  }

  bool ringStable() const {
    if (ringCount_ < RING_N) {
      return false;
    }
    float lo = ring_[0];
    float hi = ring_[0];
    for (int i = 1; i < RING_N; i++) { // bounded
      if (ring_[i] < lo) {
        lo = ring_[i];
      }
      if (ring_[i] > hi) {
        hi = ring_[i];
      }
    }
    return (hi - lo) < tun_.landedBandM;
  }

  static const int RING_N = 24; // 24 samples @ 2 Hz = 12 s window

  FlightTunables tun_;
  FlightState st_ = FlightState::CALIBRATING;
  uint32_t startMs_ = 0;
  uint32_t lastMs_ = 0;

  bool altInit_ = false;
  float altF_ = 0.0f;
  float prevAltF_ = 0.0f;
  float vSpd_ = 0.0f;

  bool baselineInit_ = false;
  float baseline_ = 0.0f;
  uint32_t calibCount_ = 0;

  bool haveBaro_ = false;
  uint32_t lastBaroMs_ = 0;

  bool accelOver_ = false;
  uint32_t accelOverSince_ = 0;
  bool altOver_ = false;
  uint32_t altOverSince_ = 0;

  uint32_t launchMs_ = 0;
  uint32_t apogeeMs_ = 0;
  uint32_t landedMs_ = 0;
  float maxAgl_ = 0.0f;

  float ring_[RING_N] = {};
  int ringCount_ = 0;
  int ringHead_ = 0;
  uint32_t lastRingMs_ = 0;
};
