# Firmware logic tests

Run from the repository root on Linux, macOS or WSL with Bash and a C++17
compiler:

```sh
bash tests/run.sh
```

The default compiler is `g++`; use `CXX=clang++ bash tests/run.sh` to select
another compiler. The script builds in a temporary directory and cleans up
afterwards. No Arduino libraries or board connection are required for these tests.

The test program includes the actual `firmware/firmware.ino` and supplies small
test substitutes for Arduino, INA228, SPI FRAM and U8g2 APIs. Sensor readings,
button levels and time are controlled by the tests. Calculations are performed by the sketch,
not copied into a separate test implementation. Each case starts with reset
sketch and fake-device state.

Coverage includes startup and missing-sensor handling, measurement unit conversion,
first-sample behavior, discharge/charge integration, irregular intervals, capacity
limits, noise boundaries, smoothing, repeated 500 ms updates, debounce, scheduling,
unsigned timestamp wrap and bounded time formatting. Float comparisons allow
rounding error.

Persistence cases exercise arbitrary initial memory, both slot validation paths,
CRC coverage of every record byte, unsupported versions and invalid float counters,
periodic saves, unchanged-state write suppression, restart continuity, one/both
slots corrupted, and interrupted writes at every byte boundary while reusing an
older valid slot. Failure injection covers unavailable FRAM, startup read errors,
write-enable/write/readback failures, silent dropped writes, lost payloads, failed
final readback, initial-save retry, and successful retry.
Energy integration, operating-time fractions, save/clock rollover and sequence
rollover are also checked. The fake FRAM retains bytes across simulated restarts;
it models partial writes and API failures, not electrical SPI behavior.
See [validation results and manual checks](../docs/FRAM_VALIDATION.md).

## Arduino IDE deployment

Continue opening `firmware/firmware.ino`, selecting Arduino Nano / ATmega328P
(or your existing bootloader choice), and uploading as before. Tests are outside
the sketch folder and add no libraries, code or memory use to the Nano build.
The forward declaration in the test harness substitutes for Arduino IDE's
automatic sketch prototype generation.

## Limits

These tests validate sketch logic with simulated inputs. They do not validate
the real libraries, sensor calibration, I²C, OLED layout/page buffering, physical
button behavior, timing under load or AVR memory use. The display substitute
only records literal text for the missing-sensor assertion; it does not emulate
rendering. Invalid readings and runtime sensor disconnection have no explicit
recovery behavior in the current sketch and are not certified by this suite.

The host compiler uses its own integer sizes and floating-point behavior. In
particular, the rollover test uses the host `unsigned long` limit, which may be
64 bits; it checks the unsigned subtraction logic rather than emulating the AVR's
32-bit ABI. Keep the separate Nano compilation check and bench checks alongside
these tests. When changing hardware API usage, keep the substitutes compatible
with the actual library APIs.
