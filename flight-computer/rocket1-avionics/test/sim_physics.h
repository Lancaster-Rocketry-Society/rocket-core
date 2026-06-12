// =============================================================================
// test/sim_physics.h — synthetic flight generator for host-side testing
//
// Integrates simple 1-D vertical rocket physics and produces *sensor-level*
// observations (barometric altitude with noise/drift at 50 Hz, accelerometer
// specific-force magnitude with ADXL375-like noise and quantisation).
// Used by both sim_flight.cpp (logic tests) and firmware_smoke.cpp.
// =============================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <random>

struct SimParams {
  // Motor: specific force (what the accelerometer feels) during burn.
  float thrustAccel = 70.0f; // m/s^2  (~7.1 g)
  float burnTime = 1.2f;     // s
  float dragK = 0.0008f;     // drag accel = k * v^2  (1/m)
  float chuteDescent = 6.0f; // m/s steady descent under chute
  float padWait = 8.0f;      // s sitting on pad before ignition
  float groundWait = 40.0f;  // s sitting on ground after touchdown

  // Sensor models
  float baroNoiseStd = 0.35f;  // m  (BMP388 w/ IIR + oversampling)
  float baroDriftAmp = 0.8f;   // m  slow sinusoidal weather-ish drift
  float baroDriftPeriod = 300.0f; // s
  float accelNoiseStd = 0.8f;  // m/s^2 (ADXL375 is a noisy +/-200g part)
  float accelLsb = 0.4787f;    // m/s^2 per LSB (49 mg) quantisation
  float baroRateHz = 50.0f;    // sensor update rate (held between updates)
  unsigned seed = 42;
};

struct SimSample {
  float t = 0.0f;          // s since sim start
  float trueAlt = 0.0f;    // m above pad (truth)
  float trueVel = 0.0f;    // m/s (truth)
  bool baroValid = true;
  float baroAltAsl = 0.0f; // m ASL as the firmware would compute it
  bool accelValid = true;
  float accelMag = 9.81f;  // m/s^2 specific-force magnitude
  bool landedTruth = false;
  bool launchedTruth = false;
};

class FlightSim {
public:
  explicit FlightSim(const SimParams &p)
      : p_(p), rng_(p.seed), gauss_(0.0f, 1.0f) {
    baseAsl_ = 57.0f; // arbitrary pad elevation ASL
  }

  // Generate the sample at absolute sim time t (call with increasing t).
  // dt between calls should be the logger tick (0.01 s).
  SimSample step(float t, float dt) {
    SimSample s;
    s.t = t;

    const float tFlight = t - p_.padWait;
    float specificForce = 9.81f; // resting on pad / ground

    if (tFlight >= 0.0f && !landed_) {
      launched_ = true;
      float aKin = 0.0f; // kinematic acceleration (d v / d t)
      const float drag = p_.dragK * vel_ * vel_;
      if (tFlight < p_.burnTime) {
        // burn: thrustAccel is specific force; kinematic = SF - g - drag
        specificForce = p_.thrustAccel;
        aKin = p_.thrustAccel - 9.81f - drag;
      } else if (!apogeePassed_) {
        if (vel_ > 0.0f) {
          // coast up: decelerating under gravity + drag
          aKin = -9.81f - drag;
          specificForce = drag; // free-fall-ish: only drag is felt
        } else {
          apogeePassed_ = true; // crossed the top this tick
          trueApogeeT_ = t;
          trueApogeeAlt_ = alt_;
          aKin = (-p_.chuteDescent - vel_) * 2.0f;
          specificForce = std::fabs(9.81f + aKin);
        }
      } else {
        // descent: relax toward steady chute speed (converges in ~0.5 s)
        aKin = (-p_.chuteDescent - vel_) * 2.0f;
        specificForce = std::fabs(9.81f + aKin);
      }
      vel_ += aKin * dt;
      alt_ += vel_ * dt;
      if (apogeePassed_ && alt_ <= 0.0f) {
        alt_ = 0.0f;
        vel_ = 0.0f;
        landed_ = true;
        touchdownT_ = t;
      }
    }

    s.trueAlt = alt_;
    s.trueVel = vel_;
    s.launchedTruth = launched_;
    s.landedTruth = landed_;

    // ---- barometer model: 50 Hz updates, noise + slow drift --------------
    if (t - lastBaroT_ >= (1.0f / p_.baroRateHz) - 1e-6f) {
      lastBaroT_ = t;
      const float drift =
          p_.baroDriftAmp *
          std::sin(2.0f * 3.14159265f * t / p_.baroDriftPeriod);
      baroHeld_ = baseAsl_ + alt_ + drift + gauss_(rng_) * p_.baroNoiseStd;
    }
    s.baroAltAsl = baroHeld_;
    s.baroValid = true;

    // ---- accelerometer model: noise + 49 mg quantisation ------------------
    float m = specificForce + gauss_(rng_) * p_.accelNoiseStd;
    m = std::round(m / p_.accelLsb) * p_.accelLsb;
    if (m < 0.0f) {
      m = 0.0f;
    }
    s.accelMag = m;
    s.accelValid = true;
    return s;
  }

  float trueApogeeT() const { return trueApogeeT_; }
  float trueApogeeAlt() const { return trueApogeeAlt_; }
  float touchdownT() const { return touchdownT_; }
  float baseAsl() const { return baseAsl_; }

private:
  SimParams p_;
  std::mt19937 rng_;
  std::normal_distribution<float> gauss_;
  float alt_ = 0.0f;
  float vel_ = 0.0f;
  bool launched_ = false;
  bool apogeePassed_ = false;
  bool landed_ = false;
  float lastBaroT_ = -1.0f;
  float baroHeld_ = 57.0f;
  float baseAsl_ = 57.0f;
  float trueApogeeT_ = -1.0f;
  float trueApogeeAlt_ = -1.0f;
  float touchdownT_ = -1.0f;
};
