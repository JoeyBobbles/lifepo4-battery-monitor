#pragma once

#include <string>

constexpr int U8G2_R0 = 0;
constexpr int U8X8_PIN_NONE = 0;
inline const uint8_t u8g2_font_6x10_tr[] = {0};
inline const uint8_t u8g2_font_ncenB14_tr[] = {0};

struct U8G2_SH1106_128X64_NONAME_1_HW_I2C {
  std::string text;
  int frames = 0;
  U8G2_SH1106_128X64_NONAME_1_HW_I2C(int, int) {}
  void begin() {}
  void firstPage() { text.clear(); ++frames; }
  bool nextPage() { return false; }
  void setFont(const uint8_t*) {}
  void setCursor(int, int) {}
  void drawStr(int, int, const char* value) { text += value; }
  void print(const char* value) { text += value; }
  template <typename T> void print(T, int = 0) {}
  void drawFrame(int, int, int, int) {}
  void drawBox(int, int, int, int) {}
};
