#pragma once

constexpr int INA228_COUNT_16 = 16;

struct Adafruit_INA228 {
  bool present = true;
  float busVoltage = 12.8f;
  float currentMA = 0.0f;
  float powerMW = 0.0f;
  float shunt = 0.0f;
  float maxCurrent = 0.0f;
  int averaging = 0;
  int reads = 0;
  bool begin() { return present; }
  void setShunt(float resistance, float range) { shunt = resistance; maxCurrent = range; }
  void setAveragingCount(int count) { averaging = count; }
  float getBusVoltage_V() { ++reads; return busVoltage; }
  float getCurrent_mA() { return currentMA; }
  float getPower_mW() { return powerMW; }
};
