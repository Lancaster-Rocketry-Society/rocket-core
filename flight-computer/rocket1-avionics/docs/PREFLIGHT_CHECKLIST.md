# Pre-flight checklist — Rocket 1 logger

Written for whoever is at the launch. No laptop needed at the field, but the
**bench test below must have been done once before launch day**. If the bench
test passed, field prep is about five minutes.

## What the LED is telling you

The onboard LED (next to the USB connector) is the only interface you need.

| LED behaviour | Meaning | What to do |
| --- | --- | --- |
| Fast flicker (10 Hz) | Calibrating (~5 s) | Hold the rocket still and vertical-ish |
| Short blip every 1 s | **PAD — armed and logging** | Good to launch |
| Solid on | In flight (BOOST/COAST/DESCENT) | Nothing — it's working |
| Two blips, pause, repeat | Landed, logging at 2 Hz | Recover rocket, power off |
| **Blinks 2, long pause, repeat** | **SD card failed** | Reseat/replace card. Do not fly |
| **Blinks 3, long pause, repeat** | **Barometer (BMP388) not responding** | Check wiring. Do not fly |
| **Blinks 4, long pause, repeat** | **Accelerometer (ADXL375) not responding** | Check wiring. Do not fly |

A fatal blink code means the logger refused to arm. Flying anyway wastes the
flight — there will be no data.

## Bench test ("kitchen-table flight") — do once before launch day

Proves the real soldering, card, and battery, end to end. ~10 minutes.

- [ ] microSD card is **FAT32** (32 GB or smaller is easiest), seated in the module
- [ ] Power on (battery via screw terminal, or USB-C — not both)
- [ ] LED flickers fast for ~5 s (calibrating)
- [ ] LED switches to a blip every 1 s (PAD — sensors and card all OK)
- [ ] Shake the stack **hard** for a solid second (a vigorous two-handed shake
      reaches the 3.6 g trigger) → LED goes solid (it thinks it launched)
- [ ] Set it down still → within ~15–40 s the LED goes to double-blips (landed)
- [ ] Power off, put the card in a computer
- [ ] A new `FLIGHTnn.CSV` exists, has a header block listing both sensors and
      their I2C addresses, and contains thousands of data rows
- [ ] If you shook it, the file contains `# EVENT` lines (LAUNCH … LANDED)

If the shake doesn't trigger "launch", that alone is **not** a failure (hand
shakes are marginal against a real motor's sustained 3.6 g+). The must-pass
items are: PAD blip reached, file created, rows present, no fatal blink code.

If anything fails: the top of the CSV (or the Serial Monitor at 115200 on a
laptop) contains a boot report with an I2C scan showing exactly what the
firmware found — compare it against `docs/WIRING.md`.

## Launch day

- [ ] Card in, battery fresh/charged and **leads strain-relieved** — ignition
      shock on a loose 9 V clip is the classic way to lose power at T+0.
      Tape/zip-tie the battery and its wires to the sled
- [ ] Optional: delete old `FLIGHTnn.CSV` test files for clean numbering
      (each boot makes a new file; numbering stops at 99)
- [ ] Mount the sled — **orientation doesn't matter** (detection uses
      acceleration magnitude, not direction)
- [ ] Power on while the rocket is still being handled is fine; just keep it
      reasonably still during the ~5 s calibration flicker
- [ ] Confirm the 1 s PAD blip before walking away from the pad
- [ ] After recovery: power off, pop the card. Done

## Power notes

* Screw terminal accepts **6–21 V** (2S LiPo at 7.4 V or a 9 V box battery
  both work). A 9 V is electrically fine but mechanically the snap connector
  is the weak point — secure it.
* The logger writes events to the card the instant they happen and flushes
  every second, so even a power loss on landing impact keeps everything
  through apogee and most of descent.

## After the flight

Get the card (or just the `FLIGHTnn.CSV`) back to Morgan, or run:

```
python3 tools/analyze_flight.py FLIGHTnn.CSV --plot flight.png
```

for the apogee/speed/g summary on the spot.
