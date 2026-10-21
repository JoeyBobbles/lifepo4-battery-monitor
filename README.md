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

Capacity starts at an assumed **100% on every boot** and is not persisted.
It is a session estimate, not a measured state of charge. Current smoothing is
implemented, but runtime estimates are not output to either display or serial.

## Hardware

| Component | Purpose / status |
| --- | --- |
| Arduino Nano, ATmega328P | Controller; I²C on A4/SDA and A5/SCL |
| Adafruit INA228 breakout with shunt | Voltage/current sensing; firmware assumes 0.015 Ω and 10 A calibration range |
| SH1106 128 × 64 I²C OLED | Display using U8g2 |
| Momentary push button | D3 to ground, using the internal pull-up |
| 5 V buck converter | Regulated Nano supply |
| Two 12.8 V, 8 Ah LiFePO4 batteries in parallel | 16 Ah nominal bank |
| SPI FRAM breakout, 64 Kbit | Shown in schematic; firmware support is planned |

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
arduino-cli lib install "U8g2@2.36.19" "Adafruit INA228 Library@3.0.0" "Adafruit BusIO@1.17.4"
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
come with the AVR core; BusIO is required by INA228. No FRAM library is needed.

## Limitations and planned work

The monitor cannot protect or disconnect the battery. Capacity is clamped to
0–16 Ah and loses all history on reboot. It assumes the starting charge and
current direction/calibration are correct. Voltage readings alone do not
establish LiFePO4 state of charge. Measurements and hardware ratings still need
bench validation; compilation is not an electrical safety check.

Planned: FRAM persistence, a capacity reset control, a displayed runtime estimate,
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
