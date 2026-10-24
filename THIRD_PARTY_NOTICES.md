# Third-party material

The [MIT licence](LICENSE) covers original project contributions. It does not
replace the terms of dependencies, embedded KiCad symbols or fonts. No external
Arduino library source or precompiled firmware is bundled in this repository.
No existing third-party copyright or licence notice was removed.

| Material | Upstream terms and attribution |
| --- | --- |
| [U8g2](https://github.com/olikraus/u8g2/blob/master/LICENSE), 2.36.19 | Code: BSD-2-Clause, Oliver Kraus; fonts have separate terms |
| [Adafruit INA228](https://github.com/adafruit/Adafruit_INA228/blob/master/license.txt), 3.0.0 | BSD-3-Clause, copyright 2022 Adafruit Industries |
| [Adafruit FRAM SPI](https://github.com/adafruit/Adafruit_FRAM_SPI/blob/master/LICENSE), 2.6.2 | BSD-3-Clause, Adafruit Industries |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO/blob/master/LICENSE), 1.17.4 | MIT, copyright 2017 Adafruit Industries |
| [Arduino AVR core](https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/Arduino.h), 1.8.8, including Wire/SPI | Core and bundled library files carry their own notices, including LGPL-2.1-or-later; retain the notices for the files used |
| [KiCad standard symbol/footprint libraries](https://www.kicad.org/libraries/license/) | KiCad library contributors; CC-BY-SA-4.0 with the electronic-design exception |

The schematic embeds `Connector_Generic:Conn_01x04`, `Connector_Generic:Conn_01x08`,
`MCU_Module:Arduino_Nano_v3.x`, `Sensor_Energy:INA228` and `Switch:SW_Push`.
These use standard KiCad library identifiers. The KiCad exception permits using
library data in designs and generated diagrams without applying CC-BY-SA to
the whole project. Redistributing a library collection has different obligations.
No separate custom library or third-party board design was found.

The firmware selects these U8g2 fonts:

- `u8g2_font_6x10_tr`: public domain terminal font, according to its source header.
- `u8g2_font_ncenB14_tr`: Adobe New Century Schoolbook; its source header credits
  Adobe Systems (1984, 1987) and Digital Equipment Corporation (1988, 1991).
  The U8g2 licence contains the associated X11 font permission terms.

No conflicting restriction was identified for publishing the original project
source under MIT while retaining these external terms. Before distributing
compiled firmware, manually review and include the relevant BSD/MIT/font notices
and satisfy the Arduino core's LGPL distribution and relinking requirements.
Do not describe a linked firmware binary as exclusively MIT-licensed.
