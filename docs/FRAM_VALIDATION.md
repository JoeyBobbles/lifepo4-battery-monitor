# SPI FRAM validation — issue #4

Validated on a host on 2026-10-24. No physical Nano/FRAM assembly was available.

## Implementation and storage

`firmware/Persistence.h` owns FRAM startup, validation, restore and periodic saves;
`firmware/firmware.ino` supplies its existing consumed Ah counter and new consumed
Wh/operating seconds. Display pages, INA228 settings, capacity calculation and
D3 debounce and short-press page switching retain their existing behavior. A
subsequent requested extension adds a ten-second D3 reset with a flashing countdown
and save confirmation; the optional FRAM debug page is enabled by default.

Two 28-byte version-1 records occupy FRAM addresses 0–55. Each contains magic,
version, reserved zero bytes, uint32 sequence, float Ah/Wh, uint32 operating seconds
and CRC-32 (IEEE). Sequence comparison uses modular ordering across rollover.
Only finite, nonnegative consumption and Ah within 0–16 are accepted.

Before reusing the older slot, its magic is invalidated and read back. Payload
is written before the final magic. Only full readback matching the intended
record and its checksum advances the active slot/sequence. An interrupted save
leaves the previously active record untouched. If an entire write completed but
its verification failed, that record may still be recovered on the next boot;
it is not acknowledged as saved in RAM. Startup read transport errors disable
persistence instead of treating unreadable storage as empty.

## Automated results

`bash tests/run.sh`: **29 cases passed**, including the original 14 cases.
The harness includes the real sketch and storage logic with simulated hardware.

| Acceptance scenario | Simulated result |
| --- | --- |
| First boot, arbitrary `0xA5` contents | Rejected; zero counters saved with valid metadata/CRC |
| Periodic saves and unchanged state | Approximately 1 s timer; alternating slots; identical counters skip writing |
| Monitoring responsiveness | 500 ms measurement/display scheduling and D3 tests pass with persistence enabled |
| Restart | Ah/Wh/seconds restored; percentage derived from restored Ah; first sample only seeds integration |
| One slot corrupt | Remaining valid record selected, for either slot |
| Both slots corrupt | Counters zeroed and a new valid record saved |
| Interrupted write | All 32 possible partial-write byte counts across invalidation/payload/magic preserve the previous valid record |
| FRAM absent | One startup attempt, Serial error, monitoring/integration continue |
| Other I/O failures | Startup read error avoids writes; failed enable/write/invalidation/final readback or silently lost payload not acknowledged; later retry succeeds |
| Initial zero-state save fails | Zero counters are retried after 1 s even without changed values |
| Timer and sequence wrap | Save/operating timers and uint32 sequence rollover pass |
| Wh and operating time | Signed charge/discharge Wh, zero clamp, fractional time, missing sensor and saturation pass |
| Optional FRAM debug page | Three-page cycle when enabled, hidden when disabled; reachable without INA228; cached ID/status, failures and recovery shown without FRAM I/O during drawing |
| Reset then restart | Ten-second hold zeroes Ah/Wh/time, resets percentage and integration baseline, and survives restart after verified save; save failure retries, unavailable FRAM is indicated, cancellation and timer rollover pass |

The host uses its native `unsigned long`, potentially 64 bits; rollover tests
exercise unsigned subtraction, not an emulated AVR ABI. CRC and record-size checks
run on host; the separate AVR compile checks the target layout and library APIs.
The FRAM fake models byte writes and injected failures, not SPI or brownout physics.

## Nano compilation and memory

Arduino CLI 1.5.1, `arduino:avr@1.8.8`, U8g2 2.36.19, INA228 3.0.0,
BusIO 1.17.4 and Adafruit FRAM SPI 2.6.2. CI and README pin these versions.

```sh
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 firmware
```

| Build | Flash (30,720 bytes available) | Static SRAM (2,048 bytes available) |
| --- | --- | --- |
| `origin/main` baseline, `6b3ad58` | 24,146 bytes (78%) | 1,023 bytes (49%) |
| Issue #4 with debug page and hold reset | 29,724 bytes (96%) | 1,202 bytes (58%) |

Both builds pass. The implementation leaves **846 bytes for stack and heap**.
There are no application allocations or `String` objects. The Adafruit libraries
allocate their device objects internally; static SRAM reports exclude these
allocations and the call stack. Storage logic uses small fixed-size records;
actual remaining SRAM under load still needs measurement on the Nano.

## Remaining bench checks and limits

- Verify the README wiring, 5 V supply, common ground, WP/HOLD levels, and device
  detection with the actual Adafruit 64 Kbit module; preserve existing I²C wiring.
- Exercise first boot, stable load/charge, repeated power cycles, deliberate slot
  corruption and power cuts during saves. Check recovered counters over Serial
  or a diagnostic FRAM read, and check OLED refresh and D3 responsiveness.
- Test startup without FRAM, runtime disconnection/reconnection and write protection;
  confirm diagnostics, continued monitoring and save retry behavior.
- Check the hold countdown/blinking, cancellation near ten seconds, one reset per
  hold, confirmation wording with working/failed FRAM, and reset followed by reboot.
- Measure current polarity/calibration, Wh accuracy, SPI behavior during brownouts,
  and actual stack/heap margin over extended operation.

Ordinary power loss discards approximately the last second since the last good
save plus a fractional operating second. Failed saves may extend that loss window;
corruption may require falling back another record. Consumption while powered off
is unobservable. The first post-boot measurement seeds integration, so that first
measurement interval is intentionally not accumulated. OLED uptime remains the
current session's uptime; stored operating time includes loop time even with no
INA228. SPI library success and readback cannot guarantee physical write integrity;
CRC collisions and faults affecting both records remain possible. Capacity still
assumes a full battery when no valid saved state exists.
