#pragma once

#include <Adafruit_FRAM_SPI.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

namespace persistence {
constexpr uint8_t CS_PIN = 10;
constexpr uint32_t MAGIC = 0x42415434UL;
constexpr uint16_t VERSION = 1;
constexpr unsigned long SAVE_INTERVAL_MS = 1000;

// Fixed 28-byte layout on AVR and host; reserved bytes must remain zero.
// Version 1 uses little-endian integers and IEEE-754 32-bit floats.
struct PersistedState {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t sequence;
  float consumedAh;
  float consumedWh;
  uint32_t operatingSeconds;
  uint32_t checksum;
};
static_assert(sizeof(PersistedState) == 28 &&
              offsetof(PersistedState, checksum) == 24, "FRAM record layout");

Adafruit_FRAM_SPI fram(CS_PIN);
bool available = false;
uint8_t activeSlot = 1;
PersistedState saved = {};
unsigned long lastSaveAttempt = 0;

uint32_t checksum(const PersistedState& state) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&state);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < offsetof(PersistedState, checksum); ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
    }
  }
  return ~crc;
}

bool valid(const PersistedState& state) {
  return state.magic == MAGIC && state.version == VERSION && state.reserved == 0 &&
         state.checksum == checksum(state) &&
         isfinite(state.consumedAh) && state.consumedAh >= 0 && state.consumedAh <= 16 &&
         isfinite(state.consumedWh) && state.consumedWh >= 0;
}

bool readSlot(uint8_t slot, PersistedState& state) {
  return fram.read(slot * sizeof(state), reinterpret_cast<uint8_t*>(&state), sizeof(state));
}

bool writeBytes(uint32_t address, const uint8_t* bytes, size_t count) {
  if (!fram.writeEnable(true)) return false;
  bool written = fram.write(address, bytes, count);
  bool disabled = fram.writeEnable(false);
  return written && disabled;
}

bool save(float consumedAh, float consumedWh, uint32_t operatingSeconds) {
  PersistedState next = {MAGIC, VERSION, 0, saved.sequence + 1,
                        consumedAh, consumedWh, operatingSeconds, 0};
  next.checksum = checksum(next);
  if (!available || !valid(next)) return false;
  uint8_t target = 1 - activeSlot;
  uint32_t address = target * sizeof(next);
  uint32_t invalidMagic = 0;
  uint32_t readbackMagic = MAGIC;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&next);
  // Invalidate the older slot and verify this before touching its payload.
  // Commit magic last. The active slot is never modified during a save.
  bool ok = writeBytes(address, reinterpret_cast<const uint8_t*>(&invalidMagic), 4) &&
            fram.read(address, reinterpret_cast<uint8_t*>(&readbackMagic), 4) &&
            readbackMagic == 0 &&
            writeBytes(address + 4, bytes + 4, sizeof(next) - 4) &&
            writeBytes(address, bytes, 4);
  PersistedState verified = {};
  ok = ok && readSlot(target, verified) && valid(verified) &&
       memcmp(&next, &verified, sizeof(next)) == 0;
  if (!ok) {
    Serial.println(F("ERROR: FRAM save failed; will retry"));
    return false;
  }
  activeSlot = target;
  saved = next;
  return true;
}

void begin(float& consumedAh, float& consumedWh, uint32_t& operatingSeconds) {
  available = fram.begin(); // One bounded attempt, no polling or delays.
  activeSlot = 1;
  saved = {};
  lastSaveAttempt = millis();
  if (!available) {
    Serial.println(F("ERROR: FRAM initialisation failed; persistence disabled"));
    return;
  }
  PersistedState first = {}, second = {};
  // A transport error is not evidence of blank memory: do not overwrite it.
  if (!readSlot(0, first) || !readSlot(1, second)) {
    available = false;
    Serial.println(F("ERROR: FRAM read failed; persistence disabled"));
    return;
  }
  bool firstValid = valid(first);
  bool secondValid = valid(second);
  if (firstValid || secondValid) {
    // Modular ordering remains correct across uint32_t sequence rollover.
    uint32_t delta = second.sequence - first.sequence;
    activeSlot = secondValid && (!firstValid || (delta != 0 && delta < 0x80000000UL)) ? 1 : 0;
    saved = activeSlot ? second : first;
    consumedAh = saved.consumedAh;
    consumedWh = saved.consumedWh;
    operatingSeconds = saved.operatingSeconds;
    Serial.println(F("FRAM counters restored"));
  } else {
    consumedAh = consumedWh = 0;
    operatingSeconds = 0;
    Serial.println(F("FRAM has no valid state; starting counters at zero"));
    save(consumedAh, consumedWh, operatingSeconds);
  }
}

void update(unsigned long now, float consumedAh, float consumedWh, uint32_t operatingSeconds) {
  if (!available || now - lastSaveAttempt < SAVE_INTERVAL_MS) return;
  lastSaveAttempt = now;
  if (saved.magic != MAGIC || consumedAh != saved.consumedAh ||
      consumedWh != saved.consumedWh || operatingSeconds != saved.operatingSeconds) {
    save(consumedAh, consumedWh, operatingSeconds);
  }
}
} // namespace persistence
