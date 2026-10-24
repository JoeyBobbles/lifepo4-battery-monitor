# Arduino LiFePO4 battery monitor

Experimental Arduino Nano monitor for two 12.8 V, 8 Ah LiFePO4 batteries in
parallel (16 Ah nominal). The firmware reads an INA228 voltage/current sensor
and drives an SH1106 128 × 64 I²C OLED.

## Current capabilities

- Samples voltage, current and power every 500 ms.
- Offers live readings/uptime and capacity pages, switched by a debounced D3 button.
- Integrates current to estimate amp-hours used and remaining; positive current
  means discharge and negative current means charge.
- Reports readings and capacity estimates at 9600 baud and displays an error
  if the INA228 is absent at startup.

Capacity resumes from the last valid SPI FRAM record. Without valid saved state,
it starts at an assumed **100%**. It remains an estimate, not a measured state of
charge. Current smoothing is implemented, but runtime estimates are not output
to either display or serial.

## Hardware

| Component | Purpose / status |
| --- | --- |
| Arduino Nano, ATmega328P | Controller; I²C on A4/SDA and A5/SCL |
| Adafruit INA228 breakout with shunt | Voltage/current sensing; firmware assumes 0.015 Ω and 10 A calibration range |
| SH1106 128 × 64 I²C OLED | Display using U8g2 |
| Momentary push button | D3 to ground, using the internal pull-up |
| 5 V buck converter | Regulated Nano supply |
| Two 12.8 V, 8 Ah LiFePO4 batteries in parallel | 16 Ah nominal bank |
| Adafruit SPI FRAM breakout, 64 Kbit (8 KB) | Saves consumed Ah, Wh and operating seconds |

The 10 A calibration setting is not a verified safe continuous-current rating.
Confirm the actual breakout, shunt rating, signal levels, supply current and
common ground before wiring. See [hardware notes](docs/HARDWARE.md).
The schematic is a breadboard prototype; the PCB file is an empty placeholder.

## Files and schematics

```text
firmware/                          Arduino sketch directory
hardware/                          KiCad 9 project, schematic and empty PCB
docs/                              Hardware notes, SVG schematic and release audit
.github/workflows/firmware.yml      Nano compilation check
```

Firmware lives in [`firmware/`](firmware/); open
[`firmware/firmware.ino`](firmware/firmware.ino) in Arduino IDE.

- [KiCad project](hardware/schematic_breadboard.kicad_pro)
- [KiCad schematic](hardware/schematic_breadboard.kicad_sch)
- [Rendered SVG diagram](docs/schematic_breadboard.svg)

![Battery monitor schematic](docs/schematic_breadboard.svg)

Use KiCad 9 or a compatible newer version. All five used symbols are embedded
in the schematic; no custom symbol library is required. Standard KiCad libraries
provide `Module:Arduino_Nano` and `Package_SO:VSSOP-10_3x3mm_P0.5mm` footprints.
For SVG export, from the repository root:

```sh
kicad-cli sch export svg --output /tmp/battery-monitor-svg/ hardware/schematic_breadboard.kicad_sch
```

## Build and upload

Install [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/)
(version 1.5.1 is used in CI), then run from the repository root:

```sh
arduino-cli core update-index
arduino-cli core install arduino:avr@1.8.8
arduino-cli lib install "U8g2@2.36.19" "Adafruit INA228 Library@3.0.0" "Adafruit BusIO@1.17.4" "Adafruit FRAM SPI@2.6.2"
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 firmware
arduino-cli board list
arduino-cli upload --port /dev/ttyUSB0 --fqbn arduino:avr:nano:cpu=atmega328 firmware
```

Replace `/dev/ttyUSB0` with the detected serial port. For a Nano with the old
bootloader, use `arduino:avr:nano:cpu=atmega328old` for both compile and upload.
CI targets the standard ATmega328P Nano. Compile the whole sketch directory so
additional tabs, headers and source files are included automatically.

In Arduino IDE, install **Arduino AVR Boards**, select **Arduino Nano** and
**ATmega328P**, install the listed libraries through Library Manager, open the
sketch, select the serial port, verify, then upload. `Arduino.h`, Wire and SPI
come with the AVR core; BusIO is required by INA228 and FRAM.

## FRAM wiring and persistence

Connect the Adafruit 64 Kbit SPI FRAM module as follows. Keep the existing I²C
connections and D3 page button unchanged; all circuits share ground.

| FRAM | Nano |
| --- | --- |
| VCC | 5V |
| GND | GND |
| SCK | D13 |
| MISO | D12 |
| MOSI | D11 |
| CS | D10 |
| WP | 5V |
| HOLD | 5V |

Startup makes one FRAM initialisation attempt. An unavailable device or startup
read error is reported over Serial and disables persistence for that boot;
voltage/current monitoring continues. With no FRAM, counters begin at zero.
Both 28-byte records (addresses 0 and 28) are checked for magic, version, CRC-32
and plausible counters. The newest valid sequence is restored. If both records
are invalid, including arbitrary factory contents, zero counters are saved.
A read transport failure does not cause existing storage to be overwritten.

Changed counters save about once per second using `millis()`. Operating time
counts powered-on loop time, including when the INA228 is missing, so it normally
makes each second's record different even at zero load. Identical counters skip
writing. Ah uses the existing signed trapezoidal integration and 0–16 Ah clamp;
Wh integrates voltage × signed current, subtracts charging, and is clamped at zero.
Operating seconds saturate at the 32-bit maximum (about 136 years).

Saves alternate slots: invalidate the older slot, verify invalidation, write its
payload, then commit its magic identifier last. The previous active slot stays
intact. Library return values and a full readback/CRC comparison must succeed
before acknowledging the new record; failed saves are logged and retried at the
next save interval. Sequence ordering handles 32-bit rollover. The storage format
is versioned and targets the Nano's little-endian, 32-bit float representation.

A power cut can lose roughly the last second of counters, plus any interval of
failed saves and the fractional operating second. Recovery can fall back further
if a record is corrupt. The first measurement after restart seeds integration;
no powered-off interval is counted. Consumption while the monitor is off cannot
be measured. SPI readback detects many failures but is not a hardware write
acknowledgement or a guarantee against brownouts, bus faults or CRC collisions.

There is no battery counter reset control; D3 still only changes pages. No new
reset gesture is introduced. The OLED continues to show session uptime and the
existing pages; Wh and accumulated operating seconds are stored without adding
new display pages. See [issue #4 validation](docs/FRAM_VALIDATION.md) for simulated
results, memory use and outstanding bench checks.

## Firmware tests

Logic tests run on a computer with Bash and a C++17 compiler, without a Nano:

```sh
bash tests/run.sh
```

They exercise the existing sketch using simulated sensor, button and clock inputs.
Tests live outside `firmware/` and do not change Arduino IDE uploads or add any
dependencies to the Nano. CI runs these tests alongside the Nano compilation check.
See [test instructions and coverage limits](tests/README.md).

## Limitations and planned work

The monitor cannot protect or disconnect the battery. Capacity is clamped to
0–16 Ah; restoration requires a valid FRAM record. It assumes the starting charge
and current direction/calibration are correct. Voltage readings alone do not
establish LiFePO4 state of charge. Measurements and hardware ratings still need
bench validation; compilation is not an electrical safety check.

Planned: a capacity reset control, a displayed runtime estimate,
and verified final wiring, calibration and enclosure documentation. The KiCad
breadboard schematic and SVG already exist; they are not a production PCB design.

## Safety

This is an **experimental hobby electronics project**. It is **not a certified
Battery Management System (BMS)** and **must not be relied upon for battery
protection**. Appropriate fusing and independent battery protection are required.
LiFePO4 packs can deliver high fault currents: use suitable wiring and secure
connections, and disconnect power before modifying the circuit.

## Contributing and licence

See [CONTRIBUTING.md](CONTRIBUTING.md). Original project material is available
under the [MIT licence](LICENSE), copyright 2026 Joe Green. External libraries,
fonts and KiCad symbols retain their own terms; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). See the
[release audit](docs/RELEASE_AUDIT.md) for validation limits and manual GitHub setup.
