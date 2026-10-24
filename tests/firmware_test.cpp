#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

// Arduino IDE generates this forward declaration when building the sketch.
void drawDisplay();
#include "../firmware/firmware.ino"

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void near(float actual, float expected, float tolerance = 0.00001f) {
  if (!std::isfinite(actual) || std::fabs(actual - expected) > tolerance) {
    throw std::runtime_error("expected " + std::to_string(expected) +
                             ", got " + std::to_string(actual));
  }
}

void reset() {
  fake::now = 0;
  fake::button = HIGH;
  fake::pin = fake::mode = -1;
  Serial = FakeSerial{};
  Wire = FakeWire{};
  ina228 = Adafruit_INA228{};
  u8g2 = U8G2_SH1106_128X64_NONAME_1_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
  currentPage = 0;
  startTime = lastUpdate = lastMeasurement = lastDebounceTime = 0;
  lastButtonReading = stableButtonState = HIGH;
  voltage = current = power = usedAh = averageCurrent = previousCurrent = 0;
  remainingAh = BATTERY_CAPACITY_AH;
  batteryPercent = 100;
  sensorAvailable = averageInitialised = previousCurrentValid = false;
}

void sample(unsigned long time, float amps) {
  fake::now = time;
  ina228.currentMA = amps * 1000.0f;
  updateMeasurements(time);
}

void button(unsigned long time, bool level) {
  fake::now = time;
  fake::button = level;
  handleButton();
}

void startup() {
  fake::now = 1234;
  setup();
  require(sensorAvailable && Wire.started, "hardware initialization");
  require(Serial.baud == 9600, "serial baud");
  require(fake::pin == 3 && fake::mode == INPUT_PULLUP, "button configuration");
  near(ina228.shunt, 0.015f);
  near(ina228.maxCurrent, 10);
  require(ina228.averaging == INA228_COUNT_16, "sensor averaging");
  require(startTime == 1234 && lastMeasurement == 1234 && lastUpdate == 1234,
          "startup timestamps");
  near(usedAh, 0);
  near(remainingAh, 16);
  near(batteryPercent, 100);
}

void missingSensor() {
  ina228.present = false;
  setup();
  sample(500, 2);
  require(!sensorAvailable && ina228.reads == 0, "absent sensor must not be read");
  require(lastMeasurement == 500 && !previousCurrentValid, "absent sensor state");
  near(usedAh, 0);
  require(u8g2.text.find("INA228 NOT FOUND") != std::string::npos, "error display");
  require(Serial.output.find("ERROR: INA228 not detected") != std::string::npos,
          "error diagnostics");
}

void firstSampleAndUnits() {
  sensorAvailable = true;
  ina228.busVoltage = 13.2f;
  ina228.powerMW = 26400;
  sample(500, 2);
  near(voltage, 13.2f);
  near(current, 2);
  near(power, 26.4f);
  near(usedAh, 0);
  require(previousCurrentValid, "first sample seeds integration");
}

void discharge() {
  sensorAvailable = true;
  sample(0, 2);
  sample(1800000, 2);
  near(usedAh, 1);
  near(remainingAh, 15);
  near(batteryPercent, 93.75f);
}

void charge() {
  sensorAvailable = true;
  usedAh = 4;
  sample(0, -2);
  sample(1800000, -2);
  near(usedAh, 3);
  near(remainingAh, 13);
  near(batteryPercent, 81.25f);
}

void changingCurrent() {
  sensorAvailable = true;
  sample(0, 1);
  sample(900000, 3);   // Mean 2 A over 0.25 hours = 0.5 Ah.
  sample(2700000, 1);  // Mean 2 A over 0.5 hours = 1 Ah.
  near(usedAh, 1.5f);
}

void capacityLimits() {
  sensorAvailable = true;
  sample(0, 10);
  sample(7200000, 10);
  near(usedAh, 16);
  near(remainingAh, 0);
  near(batteryPercent, 0);
  sample(7200000, -10);
  sample(14400000, -10);
  near(usedAh, 0);
  near(remainingAh, 16);
  near(batteryPercent, 100);
}

void noiseBoundary() {
  sensorAvailable = true;
  sample(0, 0.0049f);
  near(current, 0);
  sample(500, -0.0049f);
  near(current, 0);
  sample(1000, 0.005f);
  near(current, 0.005f);
  sample(1500, -0.005f);
  near(current, -0.005f);
}

void smoothing() {
  sensorAvailable = true;
  sample(0, 2);
  near(averageCurrent, 2);
  sample(300000, 4);
  near(averageCurrent, 3);
  sample(600000, -2);
  near(averageCurrent, 1.5f);
  float before = usedAh;
  sample(600000, 10);
  near(averageCurrent, 1.5f);
  near(usedAh, before);
}

void repeatedSmallIntervals() {
  sensorAvailable = true;
  sample(0, 2);
  for (unsigned long time = 500; time <= 1800000; time += 500) sample(time, 2);
  near(usedAh, 1, 0.0001f);
  near(remainingAh, 15, 0.0001f);
}

void debounce() {
  button(100, LOW);
  button(110, HIGH);
  button(120, LOW);
  button(170, LOW);
  require(currentPage == 0, "exactly 50 ms must not trigger");
  button(171, LOW);
  require(currentPage == 1, "settled press changes page");
  button(1000, LOW);
  require(currentPage == 1, "held press must not repeat");
  button(1100, HIGH);
  button(1151, HIGH);
  require(currentPage == 1, "release must not change page");
  button(1200, LOW);
  button(1251, LOW);
  require(currentPage == 0, "second press wraps page");
}

void scheduling() {
  setup();
  fake::now = 499;
  loop();
  require(ina228.reads == 0, "must wait for interval");
  fake::now = 500;
  loop();
  require(ina228.reads == 1 && lastUpdate == 500, "sample at interval");
  fake::now = 1700;
  loop();
  require(ina228.reads == 2 && lastUpdate == 1700, "delayed loop samples once");
  loop();
  require(ina228.reads == 2, "same timestamp must not sample again");
}

void unsignedClockWrap() {
  // Exercises unsigned subtraction at the host unsigned-long boundary.
  // The host may use 64 bits; this does not emulate the AVR's 32-bit ABI.
  sensorAvailable = true;
  const auto maximum = std::numeric_limits<unsigned long>::max();
  lastMeasurement = maximum - 249;
  previousCurrent = 2;
  previousCurrentValid = true;
  sample(250, 2); // 500 ms across rollover.
  near(usedAh, 2.0f * 500 / 3600000, 0.0000001f);
  lastUpdate = maximum - 249;
  loop();
  require(lastUpdate == 250, "scheduler across rollover");
  lastDebounceTime = maximum - 24;
  lastButtonReading = LOW;
  button(26, LOW); // 51 ms across rollover.
  require(currentPage == 1, "debounce across rollover");
}

void timeFormatting() {
  char buffer[16];
  formatTime(0, buffer, sizeof(buffer));
  require(std::string(buffer) == "00:00:00", "zero time");
  formatTime(3661, buffer, sizeof(buffer));
  require(std::string(buffer) == "01:01:01", "hours minutes seconds");
  formatTime(360000, buffer, sizeof(buffer));
  require(std::string(buffer) == "100:00:00", "hours beyond two digits");
  char small[5] = {};
  formatTime(3661, small, sizeof(small));
  require(std::string(small) == "01:0", "bounded output");
  char untouched = 'x';
  formatTime(0, &untouched, 0);
  require(untouched == 'x', "zero-sized buffer must not be written");
}

int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[] = {
    {"startup", startup}, {"missing sensor", missingSensor},
    {"first sample and units", firstSampleAndUnits}, {"discharge", discharge},
    {"charge", charge}, {"changing current", changingCurrent},
    {"capacity limits", capacityLimits}, {"noise boundary", noiseBoundary},
    {"smoothing and zero elapsed time", smoothing},
    {"repeated 500 ms intervals", repeatedSmallIntervals},
    {"button debounce", debounce}, {"scheduling", scheduling},
    {"unsigned clock wrap", unsignedClockWrap}, {"time formatting", timeFormatting}
  };
  int failures = 0;
  for (const auto& test : tests) {
    reset();
    try {
      test.run();
      std::cout << "PASS " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
    }
  }
  return failures ? 1 : 0;
}
