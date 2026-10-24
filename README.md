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
| SparkFun INA228 current sensor (Qwiic) with shunt | Voltage/current sensing; firmware assumes 0.015 Ω and 10 A calibration range |
| SH1106 128 × 64 I²C OLED | Display using U8g2 |
| Momentary push button | D3 to ground, using the internal pull-up |
| 5 V buck converter | Regulated Nano supply |
| Two 12.8 V, 8 Ah LiFePO4 batteries in parallel | 16 Ah nominal bank |
| Adafruit SPI FRAM breakout, 64 Kbit (8 KB) | Saves consumed Ah, Wh and operating seconds |

The 10 A calibration setting is not a verified safe continuous-current rating.
Confirm the actual breakout, shunt rating, signal levels, supply current and
common ground before wiring. See [hardware notes](docs/HARDWARE.md).
The schematic is a breadboard prototype; the PCB file is an empty placeholder.

## Bill of Materials (BoM)

Parts used for this build are listed below. See the [detailed BoM](docs/BOM.md)
for costs, pricing sources and the project total.

### Base parts

| Part | Quantity | Supplier |
| --- | ---: | --- |
| PCB protoboard | 2 | [The Pi Hut](https://thepihut.com/products/pcb-breadboard-protoboard?variant=56083548471681) |
| Adafruit SPI FRAM module, 64 Kbit / 8 KByte | 1 | [The Pi Hut](https://thepihut.com/products/adafruit-spi-non-volatile-fram-breakout-64kbit-8kbyte?variant=27739672465) |
| SparkFun current sensor — INA228 (Qwiic) | 1 | [The Pi Hut](https://thepihut.com/products/sparkfun-current-sensor-ina228-qwiic?variant=56219426652545) |
| DC-DC buck converter, 7–24 V to 5 V / 4 A | 1 | [The Pi Hut](https://thepihut.com/products/dc-dc-buck-converter-7-24v-to-5v-4a?variant=39865627607235) |
| 1.3-inch OLED display module, 128 × 64 | 1 | [The Pi Hut](https://thepihut.com/products/1-3-oled-display-module-128x64?variant=42810024755395) |
| ELEGOO Arduino-compatible Nano V3.0 | 1 board | [ELEGOO EU (three-board pack)](https://eu.elegoo.com/en-es/products/elegoo-nano-v3-0) |

### Extra parts

| Part | Quantity | Supplier |
| --- | ---: | --- |
| 120-piece 2.54 mm breakaway PCB pin header connector strip assortment kit | 1 kit | [Amazon UK](https://www.amazon.co.uk/dp/B09NDMVD2C) |
| 310-piece M3 male/female nylon hex standoff spacer kit | 1 kit | [Amazon UK](https://www.amazon.co.uk/dp/B09P8VB2G4) |

Kit quantities refer to complete purchased assortments; only some pieces are used
in the build. The linked Nano listing supplies three boards; one is used here.
Batteries, the page button, wiring, solder and any enclosure are outside this
supplied parts list.

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

### Reset metrics

Hold D3 for **10 seconds** to reset consumed Ah, consumed Wh, accumulated operating
time and session uptime. Capacity returns to **16 Ah / 100% assumed full**; only
use this when you intend to mark the battery full. A short press still changes
pages immediately. After one second of holding, the OLED shows a countdown,
flashing `HOLD TO RESET` and `Release to cancel`. Releasing before ten seconds
cancels the reset. Monitoring and periodic saves continue during the countdown.

At ten seconds the display shows `METRICS RESET` and the FRAM result. Reset values
are saved immediately with readback verification. `FRAM saved` confirms success;
`FRAM save pending` means the reset happened in RAM and the normal save timer will
retry. With no FRAM it shows `FRAM unavailable`; a reset cannot then be guaranteed
to survive restart. Losing power before a successful save can restore old counters.
Confirmation remains while held and for three seconds after release. Holding
longer does not reset repeatedly. Consumption and operating time resume from zero;
the first post-reset sample seeds integration. Live voltage/current/power remain
available and the existing pages retain their layout.

### FRAM debug page

`bool enableFramDebugPage = true;` near the top of `firmware/firmware.ino` enables
an additional page: live → capacity → FRAM debug → live. Set it to `false` to
restore the two-page cycle. It is enabled by default for troubleshooting, and
can be reached even if the INA228 or FRAM is missing.

The page shows initialisation/read/save failures, restored or verified-save state,
cached manufacturer/product IDs in hexadecimal, the active slot/sequence, and time
since the last verified save this boot. For the MB85RS64V, the expected ID is
`4 / 302`; other supported variants may differ. `Slot: none` and `Saved: not this
boot` mean there is no acknowledged record or verified write respectively.
`Restored / ready` means a valid record was loaded, without claiming it was written
this boot. A save failure keeps the last good record details visible.

Drawing this page only uses cached diagnostics. It does not probe SPI or measure
FRAM supply voltage; status reflects the latest operation. Initialisation failure
leaves persistence disabled until reboot. Test the actual FRAM VCC and wiring to
establish the cause of a detection failure. See [issue #4 validation](docs/FRAM_VALIDATION.md) for simulated
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

Planned: a displayed runtime estimate,
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
