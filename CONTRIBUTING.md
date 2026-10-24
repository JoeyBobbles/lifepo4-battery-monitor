# Contributing

Open an issue before significant firmware, hardware or dependency changes so
the intended behaviour and electrical assumptions can be discussed first.
For defects, include reproduction steps, board/bootloader, library versions,
wiring revision and relevant serial output. Remove credentials and personal data.

Pull requests should explain the problem, the change and validation performed.
Keep changes focused, preserve third-party notices, update affected documentation,
and identify any checks or hardware tests that could not be performed.

Compile the entire sketch directory, including any additional source files,
for the Arduino Nano ATmega328P before submitting a firmware change:

```sh
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 firmware
```

Use the dependencies and installation commands in [README.md](README.md).
Run `bash tests/run.sh` to check firmware logic on a computer with a C++17
compiler; see [the test documentation](tests/README.md) for coverage and limits.
The pull request's `Compile Nano firmware` check must pass. Compilation alone
does not validate readings, timing, memory use under load or electrical safety;
describe relevant bench testing when changing these behaviours.

For hardware changes, update the KiCad project/schematic and its rendered SVG
together. Use KiCad 9 or a compatible newer version, include any new custom
symbols/footprints and portable project-relative library references, and exclude
backups, lock files and local preferences. Explain electrical connections,
pin assignments, supply voltages, level compatibility, current/shunt ratings,
power consumption, wire sizing, fusing and independent protection affected by
the change. Document measurements and unresolved assumptions. Do not silently
change electrical connections to clear validation warnings.
