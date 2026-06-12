// =============================================================================
// test/sim_flight.cpp — logic-level verification of flight_logic.h
//
// Drives the exact FlightLogic class that flies through full simulated
// flights (sim_physics.h) plus hand-built pad-abuse scenarios, and asserts:
//   * state transitions only ever follow allowed edges
//   * launch / apogee / landing are detected within tight tolerances
//   * sensor dropouts (baro or accel) never wedge or NaN the logic
//   * pad handling bumps and barometric weather drift never false-trigger
//
// Exit code 0 = all scenarios pass. Run via run_tests.sh.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <vector>

#include "../firmware/lurs_flight_computer/flight_logic.h"
#include "sim_physics.h"

static int g_fails = 0;
static const char *g_scn = "?";

#define CHECK(cond, ...)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      g_fails++;                                                               \
      printf("FAIL [%s] %s:%d: ", g_scn, __FILE__, __LINE__);                  \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
    }                                                                          \
  } while (0)

// ---------------------------------------------------------------------------
// Transition recording + allowed-edge validation
// ---------------------------------------------------------------------------
struct Transition {
  float t;
  FlightState from;
  FlightState to;
};

static bool edgeAllowed(FlightState a, FlightState b) {
  using F = FlightState;
  return (a == F::CALIBRATING && b == F::PAD) ||
         (a == F::PAD && b == F::BOOST) ||
         (a == F::BOOST && b == F::COAST) ||
         (a == F::BOOST && b == F::DESCENT) || // apogee failsafe from boost
         (a == F::COAST && b == F::DESCENT) ||
         (a == F::COAST && b == F::LANDED) || // landed failsafe
         (a == F::DESCENT && b == F::LANDED);
}

struct RunResult {
  std::vector<Transition> trans;
  FlightState finalState = FlightState::CALIBRATING;
  float launch_s = -1.0f;  // from FlightLogic bookkeeping
  float apogee_s = -1.0f;
  float landed_s = -1.0f;
  float maxAgl = 0.0f;
  float peakVspd = 0.0f;
  float minVspd = 0.0f;

  float firstTime(FlightState s) const {
    for (const auto &tr : trans) {
      if (tr.to == s) {
        return tr.t;
      }
    }
    return -1.0f;
  }
  bool reached(FlightState s) const { return firstTime(s) >= 0.0f; }
};

// ---------------------------------------------------------------------------
// Drive FlightLogic from a FlightSim at 100 Hz. Fault hooks let scenarios
// invalidate either sensor over time windows. Optional CSV dump for plots.
// ---------------------------------------------------------------------------
using FaultFn = std::function<bool(float)>; // returns "sensor valid at t?"

static RunResult runFlight(const SimParams &p, float durationS,
                           FaultFn baroValidAt = nullptr,
                           FaultFn accelValidAt = nullptr,
                           const char *csvPath = nullptr) {
  FlightSim sim(p);
  FlightLogic logic;
  logic.begin(0);

  RunResult r;
  FlightState prev = logic.state();
  FILE *csv = nullptr;
  if (csvPath != nullptr) {
    csv = fopen(csvPath, "w");
    if (csv != nullptr) {
      fprintf(csv, "t_s,state,true_alt_m,true_vel_ms,est_agl_m,est_vspd_ms,"
                   "accel_ms2\n");
    }
  }

  const int ticks = (int)(durationS * 100.0f);
  for (int i = 0; i <= ticks; i++) { // bounded
    const float t = (float)i * 0.01f;
    SimSample s = sim.step(t, 0.01f);

    const bool baroValid = baroValidAt ? baroValidAt(t) : true;
    const bool accelValid = accelValidAt ? accelValidAt(t) : true;

    logic.update((uint32_t)(t * 1000.0f), baroValid, s.baroAltAsl, accelValid,
                 s.accelMag);

    // The logic must never emit non-finite numbers, ever.
    CHECK(std::isfinite(logic.altitudeAgl()), "AGL not finite at t=%.2f", t);
    CHECK(std::isfinite(logic.verticalSpeed()), "vspd not finite at t=%.2f",
          t);

    const FlightState st = logic.state();
    if (st != prev) {
      CHECK(edgeAllowed(prev, st), "illegal transition %s->%s at t=%.2f",
            flightStateName(prev), flightStateName(st), t);
      r.trans.push_back({t, prev, st});
      prev = st;
    }
    if (logic.armed()) {
      if (logic.verticalSpeed() > r.peakVspd) {
        r.peakVspd = logic.verticalSpeed();
      }
      if (logic.verticalSpeed() < r.minVspd) {
        r.minVspd = logic.verticalSpeed();
      }
    }

    if (csv != nullptr && (i % 2) == 0) { // 50 Hz is plenty for plotting
      fprintf(csv, "%.2f,%s,%.3f,%.3f,%.3f,%.3f,%.3f\n", t,
              flightStateName(st), s.trueAlt, s.trueVel, logic.altitudeAgl(),
              logic.verticalSpeed(), s.accelMag);
    }
  }
  if (csv != nullptr) {
    fclose(csv);
  }

  r.finalState = logic.state();
  r.launch_s = (float)logic.launchTimeMs() / 1000.0f;
  r.apogee_s = (float)logic.apogeeTimeMs() / 1000.0f;
  r.landed_s = (float)logic.landedTimeMs() / 1000.0f;
  r.maxAgl = logic.maxAltitudeAgl();
  return r;
}

// ---------------------------------------------------------------------------
// Scenario: nominal flight
// ---------------------------------------------------------------------------
static void scnNominal(const char *csvPath) {
  g_scn = "nominal";
  SimParams p;
  FlightSim probe(p); // probe run to obtain ground truth
  for (int i = 0; i <= 11000; i++) {
    probe.step((float)i * 0.01f, 0.01f);
  }
  RunResult r = runFlight(p, 110.0f, nullptr, nullptr, csvPath);

  CHECK(r.reached(FlightState::PAD), "never armed");
  CHECK(r.reached(FlightState::BOOST), "launch never detected");
  CHECK(r.reached(FlightState::COAST), "burnout never detected");
  CHECK(r.reached(FlightState::DESCENT), "apogee never detected");
  CHECK(r.finalState == FlightState::LANDED, "did not finish LANDED");

  const float ignition = p.padWait;
  const float latency = r.firstTime(FlightState::BOOST) - ignition;
  CHECK(latency >= 0.09f && latency <= 0.25f,
        "launch latency %.3f s outside [0.09, 0.25]", latency);

  const float apErrT = std::fabs(r.apogee_s - probe.trueApogeeT());
  const float apErrA = std::fabs(r.maxAgl - probe.trueApogeeAlt());
  CHECK(apErrT <= 0.6f, "apogee time err %.2f s (est %.2f vs true %.2f)",
        apErrT, r.apogee_s, probe.trueApogeeT());
  CHECK(apErrA <= 5.0f, "apogee alt err %.2f m (est %.1f vs true %.1f)",
        apErrA, r.maxAgl, probe.trueApogeeAlt());

  const float dscLag = r.firstTime(FlightState::DESCENT) - probe.trueApogeeT();
  CHECK(dscLag >= 0.0f && dscLag <= 2.5f,
        "DESCENT lag after true apogee %.2f s outside [0, 2.5]", dscLag);

  const float landLag = r.firstTime(FlightState::LANDED) - probe.touchdownT();
  CHECK(landLag >= 5.0f && landLag <= 20.0f,
        "LANDED %.1f s after touchdown, outside [5, 20]", landLag);

  CHECK(r.peakVspd >= 45.0f && r.peakVspd <= 85.0f,
        "peak vspd %.1f outside [45, 85]", r.peakVspd);
  CHECK(r.minVspd <= -4.0f && r.minVspd >= -25.0f,
        "min vspd %.1f outside [-25, -4]", r.minVspd);

  printf("PASS [%s] latency=%.0fms apogee=%.1fm(err %.1fm) t_ap_err=%.2fs "
         "landed+%.1fs\n",
         g_scn, latency * 1000.0f, r.maxAgl, apErrA, apErrT, landLag);
}

// ---------------------------------------------------------------------------
// Scenario: accelerometer dead the whole time — altitude backup must launch
// ---------------------------------------------------------------------------
static void scnAccelDropout() {
  g_scn = "accel_dropout";
  SimParams p;
  FlightSim probe(p);
  for (int i = 0; i <= 11000; i++) {
    probe.step((float)i * 0.01f, 0.01f);
  }
  RunResult r = runFlight(
      p, 110.0f, nullptr, [](float) { return false; } /* accel dead */);

  CHECK(r.reached(FlightState::BOOST), "backup launch trigger never fired");
  const float latency = r.firstTime(FlightState::BOOST) - p.padWait;
  CHECK(latency > 0.0f && latency <= 2.0f,
        "backup launch latency %.2f s outside (0, 2]", latency);
  // With no accel there is no burnout signal; apogee failsafe must take the
  // BOOST -> DESCENT edge directly.
  CHECK(r.reached(FlightState::DESCENT), "apogee never detected");
  CHECK(!r.reached(FlightState::COAST), "COAST reached with a dead accel?");
  CHECK(r.finalState == FlightState::LANDED, "did not finish LANDED");
  const float apErrT = std::fabs(r.apogee_s - probe.trueApogeeT());
  CHECK(apErrT <= 0.6f, "apogee time err %.2f s", apErrT);
  printf("PASS [%s] backup launch +%.2fs, apogee err %.2fs\n", g_scn, latency,
         apErrT);
}

// ---------------------------------------------------------------------------
// Scenario: barometer drops out for 3 s mid-coast, recovers before apogee
// ---------------------------------------------------------------------------
static void scnBaroDropoutCoast() {
  g_scn = "baro_dropout_coast";
  SimParams p;
  FlightSim probe(p);
  for (int i = 0; i <= 11000; i++) {
    probe.step((float)i * 0.01f, 0.01f);
  }
  RunResult r = runFlight(p, 110.0f,
                          [](float t) { return !(t >= 12.0f && t < 15.0f); });

  CHECK(r.finalState == FlightState::LANDED, "did not finish LANDED");
  CHECK(r.reached(FlightState::DESCENT), "apogee never detected");
  const float apErrT = std::fabs(r.apogee_s - probe.trueApogeeT());
  CHECK(apErrT <= 0.7f, "apogee time err %.2f s after dropout", apErrT);
  printf("PASS [%s] apogee err %.2fs despite 3 s baro outage\n", g_scn,
         apErrT);
}

// ---------------------------------------------------------------------------
// Scenario: barometer outage spanning apogee itself (13 s .. 20 s).
// Apogee timing is unavoidably late; the requirement is graceful recovery:
// detect descent soon after data returns, still land, never NaN.
// ---------------------------------------------------------------------------
static void scnBaroDropoutApogee() {
  g_scn = "baro_dropout_apogee";
  SimParams p;
  FlightSim probe(p);
  for (int i = 0; i <= 11000; i++) {
    probe.step((float)i * 0.01f, 0.01f);
  }
  const float recover = 20.0f;
  RunResult r = runFlight(
      p, 110.0f, [=](float t) { return !(t >= 13.0f && t < recover); });

  CHECK(r.reached(FlightState::DESCENT), "never recovered into DESCENT");
  const float dsc = r.firstTime(FlightState::DESCENT);
  CHECK(dsc >= recover && dsc <= recover + 2.5f,
        "DESCENT at %.2f s, expected within 2.5 s of recovery (%.1f s)", dsc,
        recover);
  CHECK(r.finalState == FlightState::LANDED, "did not finish LANDED");
  CHECK(r.maxAgl <= probe.trueApogeeAlt() + 5.0f &&
            r.maxAgl >= probe.trueApogeeAlt() - 40.0f,
        "recorded max AGL %.1f implausible vs true %.1f", r.maxAgl,
        probe.trueApogeeAlt());
  printf("PASS [%s] recovered: DESCENT %.1fs, max AGL %.0fm (true %.0fm)\n",
         g_scn, dsc, r.maxAgl, probe.trueApogeeAlt());
}

// ---------------------------------------------------------------------------
// Scenario: weak motor (30 m/s^2 < 35 m/s^2 accel threshold) — the altitude
// backup trigger must still catch the launch quickly.
// ---------------------------------------------------------------------------
static void scnWeakMotor() {
  g_scn = "weak_motor";
  SimParams p;
  p.thrustAccel = 30.0f;
  FlightSim probe(p);
  for (int i = 0; i <= 11000; i++) {
    probe.step((float)i * 0.01f, 0.01f);
  }
  RunResult r = runFlight(p, 110.0f);

  CHECK(r.reached(FlightState::BOOST), "weak-motor launch never detected");
  const float latency = r.firstTime(FlightState::BOOST) - p.padWait;
  CHECK(latency > 0.0f && latency <= 2.5f,
        "weak-motor launch latency %.2f s outside (0, 2.5]", latency);
  CHECK(r.finalState == FlightState::LANDED, "did not finish LANDED");
  CHECK(r.maxAgl > 15.0f, "max AGL %.1f m implausibly low", r.maxAgl);
  printf("PASS [%s] launch +%.2fs, apogee %.0fm (true %.0fm)\n", g_scn,
         latency, r.maxAgl, probe.trueApogeeAlt());
}

// ---------------------------------------------------------------------------
// Scenario: pad abuse — handling bumps, short accel spikes, a pressure
// transient (door slam / wind gust). None may trigger a launch. A genuinely
// sustained 4 g push (positive control) must.
// ---------------------------------------------------------------------------
static void scnPadBumps() {
  g_scn = "pad_bumps";
  FlightLogic logic;
  logic.begin(0);
  std::mt19937 rng(7);
  std::normal_distribution<float> noise(0.0f, 0.2f);

  FlightState prev = logic.state();
  bool boosted = false;
  float boostT = -1.0f;

  for (int i = 0; i <= 2800; i++) { // 28 s @ 100 Hz, bounded
    const float t = (float)i * 0.01f;
    float baro = 57.0f + noise(rng);
    float acc = 9.81f + noise(rng);

    if (t >= 10.0f && t < 10.5f) {
      acc = 30.0f; // long but BELOW the 35 m/s^2 threshold
    }
    if (t >= 12.0f && t < 12.06f) {
      acc = 80.0f; // hard knock, 60 ms < 100 ms hold
    }
    if (t >= 14.0f && t < 14.09f) {
      acc = 50.0f; // 90 ms, still under the hold time
    }
    if (t >= 16.0f && t < 16.2f) {
      baro += 25.0f; // 200 ms pressure transient < 300 ms hold
    }
    if (t >= 26.0f && t < 26.15f) {
      acc = 40.0f; // positive control: sustained > threshold
    }

    logic.update((uint32_t)(t * 1000.0f), true, baro, true, acc);
    const FlightState st = logic.state();
    if (st != prev) {
      CHECK(edgeAllowed(prev, st), "illegal transition %s->%s",
            flightStateName(prev), flightStateName(st));
      prev = st;
    }
    if (t < 25.0f) {
      CHECK(st == FlightState::CALIBRATING || st == FlightState::PAD,
            "false launch at t=%.2f", t);
    }
    if (!boosted && st == FlightState::BOOST) {
      boosted = true;
      boostT = t;
    }
  }
  CHECK(boosted, "positive control: sustained 4 g never triggered");
  CHECK(boostT >= 26.1f && boostT <= 26.2f,
        "positive control fired at %.2f s, expected ~26.11", boostT);
  printf("PASS [%s] bumps rejected, positive control at %.2fs\n", g_scn,
         boostT);
}

// ---------------------------------------------------------------------------
// Scenario: 15 minutes on the pad with +4 m of barometric weather drift.
// The baseline tracker must follow it; AGL must stay near zero; never launch.
// ---------------------------------------------------------------------------
static void scnPadDrift() {
  g_scn = "pad_drift";
  FlightLogic logic;
  logic.begin(0);
  std::mt19937 rng(11);
  std::normal_distribution<float> noise(0.0f, 0.35f);

  float maxAgl = 0.0f;
  for (int i = 0; i <= 90000; i++) { // 900 s @ 100 Hz, bounded
    const float t = (float)i * 0.01f;
    const float drift = 4.0f * (t / 900.0f);
    const float baro = 57.0f + drift + noise(rng);
    logic.update((uint32_t)(t * 1000.0f), true, baro, true,
                 9.81f + noise(rng));
    CHECK(logic.state() == FlightState::CALIBRATING ||
              logic.state() == FlightState::PAD,
          "false launch from weather drift at t=%.1f", t);
    if (logic.armed()) {
      const float agl = std::fabs(logic.altitudeAgl());
      if (agl > maxAgl) {
        maxAgl = agl;
      }
    }
  }
  CHECK(maxAgl < 5.0f, "AGL drifted to %.2f m on the pad", maxAgl);
  printf("PASS [%s] 15 min drift, max |AGL| %.2fm\n", g_scn, maxAgl);
}

// ---------------------------------------------------------------------------
// Scenario: nominal flight repeated across 12 RNG seeds — detection must not
// depend on one lucky noise realisation.
// ---------------------------------------------------------------------------
static void scnSeedSweep() {
  g_scn = "seed_sweep";
  for (unsigned seed = 1; seed <= 12; seed++) {
    SimParams p;
    p.seed = seed;
    FlightSim probe(p);
    for (int i = 0; i <= 11000; i++) {
      probe.step((float)i * 0.01f, 0.01f);
    }
    RunResult r = runFlight(p, 110.0f);
    CHECK(r.finalState == FlightState::LANDED, "seed %u: not LANDED", seed);
    const float apErrT = std::fabs(r.apogee_s - probe.trueApogeeT());
    const float apErrA = std::fabs(r.maxAgl - probe.trueApogeeAlt());
    CHECK(apErrT <= 1.0f, "seed %u: apogee time err %.2f s", seed, apErrT);
    CHECK(apErrA <= 5.0f, "seed %u: apogee alt err %.2f m", seed, apErrA);
    const float latency = r.firstTime(FlightState::BOOST) - p.padWait;
    CHECK(latency >= 0.05f && latency <= 0.3f, "seed %u: latency %.3f s",
          seed, latency);
  }
  printf("PASS [%s] 12 seeds: all landed, apogee within 1.0s / 5m\n", g_scn);
}

// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
  const char *csvPath = (argc > 1) ? argv[1] : nullptr;
  scnNominal(csvPath);
  scnSeedSweep();
  scnAccelDropout();
  scnBaroDropoutCoast();
  scnBaroDropoutApogee();
  scnWeakMotor();
  scnPadBumps();
  scnPadDrift();

  if (g_fails == 0) {
    printf("ALL LOGIC TESTS PASSED\n");
    return 0;
  }
  printf("%d FAILURE(S)\n", g_fails);
  return 1;
}
