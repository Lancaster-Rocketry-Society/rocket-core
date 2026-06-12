# Wiring — what the firmware assumes

The stack was already soldered before this firmware was written, so the pin
map below is the **assumed** wiring, not a verified one. Everything
hardware-specific is in `firmware/lurs_flight_computer/config.h` — if the
real board differs, change it there and re-flash. The bench test in
`PREFLIGHT_CHECKLIST.md` is what confirms the assumptions.

Pin labels are identical on the Nano ESP32 and the Nano 33 IoT, so this
table applies to either board.

## I2C bus (shared by both sensors)

| Sensor pin | Nano pin | Notes |
| --- | --- | --- |
| BMP388 SDA | A4 | shared bus |
| BMP388 SCL | A5 | shared bus |
| BMP388 VIN | 3.3V | breakout has its own regulator; 3.3 V is fine |
| BMP388 GND | GND | |
| ADXL375 SDA | A4 | same bus |
| ADXL375 SCL | A5 | same bus |
| ADXL375 VCC | 3.3V | |
| ADXL375 CS | 3.3V (high) | ties the chip into I2C mode |
| ADXL375 GND | GND | |

I2C addresses are **not** assumed: the firmware probes the primary address,
then the secondary (`0x77`/`0x76` for the BMP388, `0x53`/`0x1D` for the
ADXL375), retries, and prints what it found. A wrong SDO/address-jumper
level shows up in the boot report instead of failing silently.

## SPI (microSD module)

| SD module pin | Nano pin | config.h |
| --- | --- | --- |
| CS   | **D10** | `PIN_SD_CS` — the one most worth double-checking |
| MOSI | D11 | hardware SPI, fixed |
| MISO | D12 | hardware SPI, fixed |
| SCK  | D13 | hardware SPI, fixed |
| VCC  | 5V/VBUS (module has a regulator + level shifter) | |
| GND  | GND | |

MOSI/MISO/SCK are the hardware SPI pins and can't be moved in software.
CS is just a GPIO — if it's actually soldered to a different pin, edit
`PIN_SD_CS` in `config.h`, re-flash, done.

Note: D13 is also the onboard LED pin the firmware uses for status. Sharing
D13 between SPI SCK and the LED is normal on Nano-format boards — the LED
flickering during SD writes is expected and harmless.

## Power

* Battery into the screw terminal → Vin. The perfboard is marked
  **6–21 V**, which matches the Nano ESP32's Vin range; a 2S LiPo (7.4 V)
  or a 9 V box battery both work electrically (see the checklist about 9 V
  clips mechanically).
* USB-C can power the stack on the bench instead — don't connect both at
  the same time unless you know the board's power-OR'ing is healthy.

## How the firmware tells you the wiring is wrong

On every boot it prints over Serial (115200) **and** writes into the log
file header:

* firmware version + selected board family,
* an I2C bus scan (every ACKing address),
* which address each sensor was actually found on,
* SD card status and the chosen `FLIGHTnn.CSV` filename.

So even without a laptop at the field, the last log file on the card shows
exactly what the firmware saw at power-up. If a peripheral is missing it
refuses to arm and blinks the matching error code (see the checklist) —
it will never sit silently on the pad logging nothing.
