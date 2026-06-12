# LURS Rocket 1 — Flight Data Logger

Standalone avionics firmware for Rocket 1: samples a BMP388 barometer and an
ADXL375 high-g accelerometer at 100 Hz, runs a flight state machine, and logs
everything to microSD as `FLIGHTnn.CSV`. Recovery on this rocket is motor
ejection, so the logger controls nothing — it only records. A logger fault
costs data, not the airframe.

```
firmware/lurs_flight_computer/   the sketch (flash this)
  lurs_flight_computer.ino       main: sensors, SD logging, LED, loop
  flight_logic.h                 portable state machine (host-tested as-is)
  config.h                       pins, addresses, rates — check before flashing
test/                            host-side verification (no hardware needed)
  run_tests.sh                   builds + runs everything
  sim_flight.cpp                 logic tests against simulated flights
  firmware_smoke.cpp             runs the unmodified .ino against mocks
  sim_physics.h, mocks/          flight simulator + Arduino mock environment
tools/analyze_flight.py          post-flight summary, plots, OpenRocket export
docs/                            wiring + launch-day checklist
```

## ⚠ Which board is this actually?

The parts list says **Arduino Nano 33 IoT**, but the board in the avionics
photo is an **Arduino Nano ESP32** (it's printed on the silkscreen, and the
radio module is a u-blox NORA-W106, which is the ESP32-S3 part). The two look
nearly identical and use the same pin labels.

The firmware supports **both** — it sticks to the common Arduino API and
handles the one real difference (ESP32 SD paths need a leading `/`)
automatically. The only thing that matters is selecting the right board in
the IDE before flashing, because the wrong choice simply won't upload:

* Tools → Board → **Arduino Nano ESP32** (board package: "Arduino ESP32
  Boards") — this is what the photo shows
* or Tools → Board → **Arduino Nano 33 IoT** (board package: "Arduino SAMD
  Boards") if that's what is actually mounted

## Build & flash (Arduino IDE 2.x)

1. Install the board package for the board you have (Boards Manager).
2. Library Manager → install **Adafruit BMP3XX Library** and **Adafruit
   ADXL375** (accept the prompt to install their dependencies:
   Adafruit Unified Sensor + Adafruit BusIO). SD and SPI ship with the core.
3. Open `firmware/lurs_flight_computer/lurs_flight_computer.ino`.
4. Check `config.h` against the real wiring — especially `PIN_SD_CS`
   (assumed D10; see `docs/WIRING.md`).
5. Select board + port, Upload.
6. Open Serial Monitor at 115200 to watch the boot report. The same report
   is written into the log file header, so the stack can also be verified
   with nothing but a card reader.

## What the logger does

```
CALIBRATING (~5 s, keep still) → PAD → BOOST → COAST → DESCENT → LANDED
```

* **Launch**: |accel| > 35 m/s² held 100 ms, **or** +20 m altitude held
  300 ms (backup — covers a weak motor or a dead accelerometer).
* **Burnout**: |accel| < 20 m/s² (earliest 300 ms after launch).
* **Apogee**: filtered altitude falls 3 m below the running maximum.
* **Landed**: altitude span < 2.5 m over a rolling 12 s window; logging then
  drops to 2 Hz until power-off. Events (`LAUNCH`, `APOGEE`, `LANDED`, …)
  are flushed to the card the moment they happen.
* Pad baseline tracks slow weather drift and freezes at launch, so hours of
  pad time can't fake altitude.
* Both sensors are tried on both I2C addresses with retries; a boot I2C scan
  and the chosen addresses are written into the log header.
* If the SD card or either sensor is dead at boot, the firmware refuses to
  arm and blinks an error code forever (see `docs/PREFLIGHT_CHECKLIST.md`) —
  never fly with a dead logger.

Coding style follows the society's safety-critical standards: no dynamic
allocation (no `String`), no recursion, every loop bounded, fixed buffers.

## How it was verified (and what that does and doesn't prove)

The state machine and the **unmodified sketch** are compiled and run on a
host machine against a physics simulator with realistic sensor models
(barometer: 50 Hz, σ≈0.35 m noise, slow weather drift; accelerometer: σ≈0.8
m/s², 49 mg quantisation). `cd test && ./run_tests.sh` reproduces all of it:

| Scenario | Result |
| --- | --- |
| Nominal flight (~260 m apogee) | launch detected in 110 ms; apogee within 0.5 m / 0.13 s of truth; landing detected 11.3 s after touchdown |
| 12 RNG seeds | all land; apogee within 1 s / 5 m every time |
| Accelerometer dead all flight | altitude backup launches in 1.15 s; full flight still logged |
| Barometer out 3 s mid-coast | apogee still within 0.13 s |
| Barometer out *across* apogee | recovers into DESCENT within 0.6 s of data returning, still lands |
| Weak motor (3 g < 3.6 g threshold) | backup trigger launches in 1.8 s |
| Pad abuse (knocks to 8 g, pressure transients) | no false launch; sustained 4 g control does trigger |
| 15 min pad wait, +4 m weather drift | max apparent altitude 0.64 m, no false launch |

The smoke test additionally proves the sketch itself: full flight produces
7,150 CSV rows with events in order and no NaNs; secondary-I2C-address
fallback works; `FLIGHT02.CSV` is chosen when 00/01 exist; each dead
peripheral produces its correct fatal code. `cppcheck` runs clean.

**Honest limits**: simulation verifies the logic, not the soldering. It
cannot prove the CS pin is really D10, that the card is FAT32, or that the
battery survives ignition shock. That's what the 10-minute bench test in
`docs/PREFLIGHT_CHECKLIST.md` is for — it must be done once on the real
stack before flight day.

## Known limitations (fine for Rocket 1, revisit later)

* No I2C bus-recovery watchdog: a sensor that hard-wedges the bus mid-flight
  would stall logging (boot failures are caught; in-flight bus lockup is
  rare and this is a logger, not a controller).
* A barometer that freezes while still reporting "success" could classify
  landing early — logging continues at 2 Hz, so data is reduced, not lost.
* The ISA pressure→altitude conversion is uncalibrated for weather; absolute
  altitude (ASL) is approximate. Altitude above pad (AGL) — what's actually
  analysed — cancels this out.
* Near/above transonic speeds the baro reading gets distorted by shock
  effects. Irrelevant at Rocket 1 speeds (~Mach 0.2), worth remembering for
  faster airframes.

## After the flight

```
python3 tools/analyze_flight.py FLIGHT00.CSV --plot flight.png --export ork.csv
```

prints apogee / max speed / max g / boost duration / descent rate / flight
time, saves plots, and exports a time-aligned CSV to overlay against an
OpenRocket simulation export — that comparison (real drag vs predicted,
actual motor performance, descent rate vs chute sizing) is the feedback loop
into the next airframe design.
