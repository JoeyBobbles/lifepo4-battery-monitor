#pragma once
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdint>

struct Adafruit_FRAM_SPI {
  std::array<uint8_t, 8192> memory;
  bool present = true;
  bool readFailure = false;
  bool writeFailure = false;
  bool enableFailure = false;
  bool silentlyDrop = false;
  bool enabled = false;
  int bytesUntilFailure = -1;
  int writes = 0;
  int dropWrite = -1;
  int reads = 0;
  int failRead = -1;
  int begins = 0;
  explicit Adafruit_FRAM_SPI(int8_t) { memory.fill(0xA5); }
  bool begin(uint8_t = 2) { ++begins; return present; }
  bool writeEnable(bool value) {
    if (!present || enableFailure) return false;
    enabled = value;
    return true;
  }
  bool read(uint32_t address, uint8_t* values, size_t count) {
    ++reads;
    if (!present || readFailure || reads == failRead || address + count > memory.size()) return false;
    std::memcpy(values, memory.data() + address, count);
    return true;
  }
  bool write(uint32_t address, const uint8_t* values, size_t count) {
    ++writes;
    if (!present || !enabled || writeFailure || address + count > memory.size()) return false;
    if (silentlyDrop || writes == dropWrite) return true;
    for (size_t i = 0; i < count; ++i) {
      if (bytesUntilFailure == 0) return false;
      memory[address + i] = values[i];
      if (bytesUntilFailure > 0) --bytesUntilFailure;
    }
    return true;
  }
};
