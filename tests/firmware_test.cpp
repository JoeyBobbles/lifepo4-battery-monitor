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
  persistence::fram = Adafruit_FRAM_SPI(persistence::CS_PIN);
  persistence::available = false;
  persistence::saved = {};
  persistence::activeSlot = 1;
  persistence::lastSaveAttempt = 0;
  usedWh = previousSignedPower = 0;
  operatingSeconds = operatingRemainderMs = lastOperatingUpdate = 0;
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

void restart() {
  auto memory = persistence::fram.memory;
  reset();
  persistence::fram.memory = memory;
  setup();
}

void blankFram() {
  setup();
  require(persistence::fram.begins == 1, "single bounded initialization");
  require(persistence::available, "FRAM enabled");
  persistence::PersistedState state = {};
  require(persistence::readSlot(0, state) && persistence::valid(state), "initial state saved");
  require(state.sequence == 1 && state.operatingSeconds == 0, "initial sequence and time");
  near(state.consumedAh, 0);
  near(state.consumedWh, 0);
}

void periodicPersistence() {
  setup();
  int writes = persistence::fram.writes;
  persistence::update(1000, 0, 0, 0);
  require(persistence::fram.writes == writes, "unchanged state avoids writes");
  usedAh = 2;
  usedWh = 25.6f;
  persistence::update(1999, usedAh, usedWh, 1);
  require(persistence::fram.writes == writes, "wait for save interval");
  persistence::update(2000, usedAh, usedWh, 1);
  require(persistence::activeSlot == 1 && persistence::saved.sequence == 2, "alternate slot");
  restart();
  near(usedAh, 2);
  near(usedWh, 25.6f);
  near(remainingAh, 14);
  near(batteryPercent, 87.5f);
  require(operatingSeconds == 1, "restore time");
  sample(500, 2);
  near(usedAh, 2); // First sample after boot seeds, never integrates off time.
  near(usedWh, 25.6f);
  sample(1800500, 2);
  near(usedAh, 3);
  near(usedWh, 38.4f);
}

void slotCorruption() {
  setup();
  require(persistence::save(3, 38.4f, 10), "second slot saved");
  persistence::fram.memory[sizeof(persistence::PersistedState) + 12] ^= 0x01;
  restart();
  near(usedAh, 0);
  require(persistence::save(4, 50, 20), "save after fallback");
  persistence::fram.memory[0] ^= 0x01;
  restart();
  near(usedAh, 4); // Invalid first slot, valid second slot.
  persistence::fram.memory[sizeof(persistence::PersistedState)] ^= 0x01;
  restart();
  near(usedAh, 0);
  near(usedWh, 0);
  require(operatingSeconds == 0 && persistence::valid(persistence::saved), "both corrupt reseeded");
}

void recordValidation() {
  persistence::PersistedState state = {persistence::MAGIC, persistence::VERSION, 0, 5, 2, 25, 10, 0};
  state.checksum = persistence::checksum(state);
  require(persistence::valid(state), "valid record");
  for (size_t i = 0; i < sizeof(state); ++i) {
    auto damaged = state;
    reinterpret_cast<uint8_t*>(&damaged)[i] ^= 1;
    require(!persistence::valid(damaged), "each byte covered by validation");
  }
  state.version++;
  state.checksum = persistence::checksum(state);
  require(!persistence::valid(state), "unsupported version");
  state.version = persistence::VERSION;
  for (float invalid : {-1.0f, 17.0f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    state.consumedAh = invalid;
    state.checksum = persistence::checksum(state);
    require(!persistence::valid(state), "invalid Ah rejected even with correct checksum");
  }
  state.consumedAh = 0;
  state.consumedWh = std::numeric_limits<float>::quiet_NaN();
  state.checksum = persistence::checksum(state);
  require(!persistence::valid(state), "invalid Wh rejected");
}

void interruptedWrites() {
  setup();
  require(persistence::save(1, 12.8f, 10), "seed second slot");
  auto original = persistence::fram.memory;
  // Every byte boundary in invalidation, payload, and final magic, including
  // reuse of a previously valid slot, must preserve the active record.
  for (int cutoff = 0; cutoff < 32; ++cutoff) {
    reset();
    persistence::fram.memory = original;
    setup();
    persistence::fram.bytesUntilFailure = cutoff;
    require(!persistence::save(2, 25.6f, 20), "partial save must fail");
    require(persistence::saved.sequence == 2 && persistence::activeSlot == 1,
            "failed save not acknowledged");
    restart();
    near(usedAh, 1);
    near(usedWh, 12.8f);
    require(operatingSeconds == 10, "recover prior record after torn write");
  }
}

void framFailures() {
  persistence::fram.present = false;
  setup();
  require(!persistence::available, "absent FRAM disabled");
  require(Serial.output.find("FRAM initialisation failed") != std::string::npos, "FRAM diagnostic");
  sample(0, 2);
  sample(1800000, 2);
  near(usedAh, 1);
  reset();
  persistence::fram.readFailure = true;
  setup();
  require(!persistence::available && persistence::fram.writes == 0, "read failure must not erase");
  reset();
  setup();
  persistence::fram.silentlyDrop = true;
  require(!persistence::save(2, 25, 10), "readback catches silent write failure");
  persistence::fram.silentlyDrop = false;
  persistence::fram.dropWrite = persistence::fram.writes + 2; // Payload only.
  require(!persistence::save(2, 25, 10), "final readback catches lost payload");
  persistence::fram.dropWrite = -1;
  persistence::fram.failRead = persistence::fram.reads + 2; // Final verification only.
  require(!persistence::save(2, 25, 10), "final verification failure not acknowledged");
  require(persistence::saved.sequence == 1, "verified save required to advance sequence");
  persistence::fram.failRead = -1;
  persistence::fram.enableFailure = true;
  require(!persistence::save(2, 25, 10), "enable failure");
  persistence::fram.enableFailure = false;
  persistence::fram.writeFailure = true;
  require(!persistence::save(2, 25, 10), "write failure");
  persistence::fram.writeFailure = false;
  persistence::fram.readFailure = true;
  require(!persistence::save(2, 25, 10), "verification read failure");
  persistence::fram.readFailure = false;
  require(persistence::save(2, 25, 10), "retry succeeds");
  restart();
  near(usedAh, 2);
}

void initialSaveRetry() {
  persistence::fram.writeFailure = true;
  setup();
  require(persistence::available && persistence::saved.magic == 0, "initial failure not saved");
  persistence::fram.writeFailure = false;
  persistence::update(999, 0, 0, 0);
  require(persistence::saved.magic == 0, "initial retry bounded by timer");
  persistence::update(1000, 0, 0, 0);
  require(persistence::valid(persistence::saved), "unchanged zeros retried after initial failure");
  restart();
  near(usedAh, 0);
}

void energyAndOperatingTime() {
  setup();
  sample(0, 2);
  sample(1800000, 2);
  near(usedWh, 12.8f);
  sample(1800000, -2);
  sample(2700000, -2);
  near(usedWh, 6.4f);
  sample(6300000, -2);
  near(usedWh, 0);
  reset();
  setup();
  for (unsigned long time : {500UL, 1000UL, 1700UL, 1999UL, 2500UL}) {
    fake::now = time;
    loop();
  }
  require(operatingSeconds == 2 && operatingRemainderMs == 500, "fractional seconds retained");
  require(ina228.reads == 4 && u8g2.frames == 5, "measurement and display stay scheduled");
  restart();
  require(operatingSeconds == 2, "time saved by loop");
  fake::now = 500;
  loop();
  require(operatingSeconds == 2, "no powered-off interval");
  fake::now = 1000;
  loop();
  require(operatingSeconds == 3, "time resumes");
  sensorAvailable = false;
  fake::now = 2000;
  loop();
  require(operatingSeconds == 4, "powered-on time counts with missing sensor");
  operatingSeconds = UINT32_MAX - 1;
  fake::now = 4000;
  loop();
  require(operatingSeconds == UINT32_MAX, "operating time saturates");
}

void persistenceClockWrap() {
  setup();
  auto maximum = std::numeric_limits<unsigned long>::max();
  persistence::lastSaveAttempt = maximum - 499;
  lastOperatingUpdate = maximum - 499;
  fake::now = 500;
  loop();
  require(operatingSeconds == 1 && persistence::saved.operatingSeconds == 1,
          "operating and save timers survive clock wrap");
  // Adjacent sequence numbers may wrap while retaining their ordering.
  persistence::saved.sequence = UINT32_MAX - 1;
  require(persistence::save(1, 1, 1), "save maximum sequence");
  require(persistence::save(2, 2, 2), "save sequence zero");
  restart();
  near(usedAh, 2);
  require(persistence::saved.sequence == 0, "wrapped sequence is newest");
}

int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[] = {
    {"blank FRAM", blankFram}, {"periodic persistence and restart", periodicPersistence},
    {"slot corruption", slotCorruption}, {"record validation", recordValidation},
    {"interrupted writes at every byte", interruptedWrites}, {"FRAM failures and retry", framFailures},
    {"initial save retry", initialSaveRetry},
    {"energy and operating time", energyAndOperatingTime}, {"persistence clock wrap", persistenceClockWrap},
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
