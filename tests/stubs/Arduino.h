#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

constexpr bool HIGH = true;
constexpr bool LOW = false;
constexpr int INPUT_PULLUP = 2;
#define F(text) text

namespace fake {
inline unsigned long now = 0;
inline bool button = HIGH;
inline int pin = -1;
inline int mode = -1;
}

inline unsigned long millis() { return fake::now; }
inline bool digitalRead(uint8_t) { return fake::button; }
inline void pinMode(uint8_t pin, int mode) {
  fake::pin = pin;
  fake::mode = mode;
}

template <typename T> T constrain(T value, T low, T high) {
  return std::min(std::max(value, low), high);
}
using std::max;

struct FakeSerial {
  unsigned long baud = 0;
  std::string output;
  void begin(unsigned long value) { baud = value; }
  void print(const char* value) { output += value; }
  void print(float value, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", digits, double(value));
    output += buffer;
  }
  template <typename T> void print(T value) { output += std::to_string(value); }
  template <typename T> void println(T value) { print(value); output += '\n'; }
};
inline FakeSerial Serial;
