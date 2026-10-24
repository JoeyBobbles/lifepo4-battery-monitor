#pragma once
struct FakeWire {
  bool started = false;
  void begin() { started = true; }
};
inline FakeWire Wire;
