#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_INA228.h>
#include "Persistence.h"

// =====================================================
// Configuration
// =====================================================

// OLED: SH1106 128x64, single-page buffer
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

Adafruit_INA228 ina228;

// Hardware
constexpr uint8_t BUTTON_PIN = 3;

// Include a third OLED page for FRAM troubleshooting. Set false to hide it.
bool enableFramDebugPage = true;

// Battery bank: 2 x 12.8V 8Ah LiFePO4 in parallel
constexpr float BATTERY_CAPACITY_AH = 16.0f;

// Adafruit INA228 onboard shunt
constexpr float SHUNT_RESISTANCE = 0.015f;
constexpr float MAX_CURRENT = 10.0f;

// Timing
constexpr unsigned long UPDATE_INTERVAL = 500;
constexpr unsigned long DEBOUNCE_MS = 50;
constexpr unsigned long RESET_HOLD_MS = 10000;
constexpr unsigned long RESET_NOTICE_MS = 3000;

// Ignore very small current readings caused by noise
constexpr float CURRENT_NOISE_THRESHOLD = 0.005f;

// Runtime estimate smoothing (5 minutes)
constexpr float AVERAGE_TIME_SECONDS = 300.0f;

// =====================================================
// State
// =====================================================

uint8_t currentPage = 0;

unsigned long startTime = 0;
unsigned long lastUpdate = 0;
unsigned long lastMeasurement = 0;

// Button debounce
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressedAt = 0;
unsigned long resetNoticeStarted = 0;
bool resetHandled = false;
bool resetNotice = false;
bool resetSavePending = false;

// INA228 measurements
float voltage = 0.0f;
float current = 0.0f;
float power = 0.0f;

bool sensorAvailable = false;

// Battery tracking
float usedAh = 0.0f;
float usedWh = 0.0f;
uint32_t operatingSeconds = 0;
unsigned long lastOperatingUpdate = 0;
uint16_t operatingRemainderMs = 0;
float remainingAh = BATTERY_CAPACITY_AH;
float batteryPercent = 100.0f;

// Runtime calculation
float averageCurrent = 0.0f;
bool averageInitialised = false;

// Previous current for integration
float previousCurrent = 0.0f;
float previousSignedPower = 0.0f;
bool previousCurrentValid = false;

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  u8g2.begin();

  sensorAvailable = ina228.begin();

  if (sensorAvailable) {
    ina228.setShunt(SHUNT_RESISTANCE, MAX_CURRENT);
    ina228.setAveragingCount(INA228_COUNT_16);

    Serial.println(F("INA228 initialised"));
  } else {
    Serial.println(F("ERROR: INA228 not detected"));
  }

  persistence::begin(usedAh, usedWh, operatingSeconds);
  remainingAh = BATTERY_CAPACITY_AH - usedAh;
  batteryPercent = (remainingAh / BATTERY_CAPACITY_AH) * 100.0f;
  previousCurrentValid = false;

  startTime = millis();
  lastUpdate = startTime;
  lastMeasurement = startTime;
  lastOperatingUpdate = startTime;
  operatingRemainderMs = 0;

  drawDisplay();

  Serial.println(F("Battery monitor started"));
  Serial.print(F("Initial remaining Ah: "));
  Serial.println(remainingAh);
}

// =====================================================
// Button handling
// =====================================================

void resetMetrics(unsigned long now) {
  usedAh = usedWh = 0;
  remainingAh = BATTERY_CAPACITY_AH;
  batteryPercent = 100;
  operatingSeconds = operatingRemainderMs = 0;
  startTime = lastOperatingUpdate = lastMeasurement = lastUpdate = now;
  previousCurrentValid = averageInitialised = false;
  previousCurrent = previousSignedPower = averageCurrent = 0;
  // Persist immediately; a failure remains pending for the normal timed retry.
  resetSavePending = !persistence::save(usedAh, usedWh, operatingSeconds);
  persistence::lastSaveAttempt = millis();
  resetNotice = true;
  resetNoticeStarted = now;
  Serial.println(F("Battery metrics reset; assuming full battery"));
}

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > DEBOUNCE_MS) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      // One page change per press; holding for ten seconds also resets metrics.
      if (stableButtonState == LOW) {
        buttonPressedAt = now;
        resetHandled = false;
        resetNotice = false;
        currentPage = (currentPage + 1) % (enableFramDebugPage ? 3 : 2);

        Serial.print(F("Page: "));
        Serial.println(currentPage + 1);

        drawDisplay();
      } else {
        // Keep confirmation visible for three seconds after release.
        resetNoticeStarted = now;
        drawDisplay();
      }
    }
  }

  // A raw release cancels eligibility immediately, before release debounce.
  if (stableButtonState == LOW && reading == LOW && !resetHandled &&
      now - buttonPressedAt >= RESET_HOLD_MS) {
    resetHandled = true;
    resetMetrics(now);
    drawDisplay();
  }
  if (resetNotice && stableButtonState == HIGH &&
      now - resetNoticeStarted >= RESET_NOTICE_MS) {
    resetNotice = false;
    drawDisplay();
  }

  lastButtonReading = reading;
}

// =====================================================
// Battery calculations
// =====================================================

void updateMeasurements(unsigned long now) {
  if (!sensorAvailable) {
    lastMeasurement = now;
    return;
  }

  // Time since last measurement
  unsigned long elapsedMs = now - lastMeasurement;
  lastMeasurement = now;

  float elapsedHours = elapsedMs / 3600000.0f;
  float elapsedSeconds = elapsedMs / 1000.0f;

  // Read INA228
  voltage = ina228.getBusVoltage_V();
  current = ina228.getCurrent_mA() / 1000.0f;
  power = ina228.getPower_mW() / 1000.0f;

  // Remove tiny current measurement noise
  if (current > -CURRENT_NOISE_THRESHOLD &&
      current < CURRENT_NOISE_THRESHOLD) {
    current = 0.0f;
  }

  // -------------------------------------------------
  // Amp-hour tracking
  // -------------------------------------------------

  // Positive current = discharging
  // Negative current = charging
  //
  // Trapezoidal integration improves accuracy when
  // current changes between measurements.

  if (previousCurrentValid) {
    float averageIntervalCurrent =
        (previousCurrent + current) / 2.0f;

    usedAh += averageIntervalCurrent * elapsedHours;
    // INA228 power is unsigned; voltage * signed current preserves charging.
    usedWh = max(0.0f, usedWh +
        (previousSignedPower + voltage * current) * 0.5f * elapsedHours);
  }

  previousSignedPower = voltage * current;
  previousCurrent = current;
  previousCurrentValid = true;

  // Keep within nominal battery capacity
  usedAh = constrain(
    usedAh,
    0.0f,
    BATTERY_CAPACITY_AH
  );

  remainingAh = BATTERY_CAPACITY_AH - usedAh;

  batteryPercent =
      (remainingAh / BATTERY_CAPACITY_AH) * 100.0f;

  // -------------------------------------------------
  // Smoothed current for runtime estimation
  // -------------------------------------------------

  float dischargeCurrent = max(0.0f, current);

  if (!averageInitialised) {
    averageCurrent = dischargeCurrent;
    averageInitialised = true;
  } else {
    // Exponential moving average
    float alpha =
        elapsedSeconds /
        (AVERAGE_TIME_SECONDS + elapsedSeconds);

    averageCurrent +=
        alpha * (dischargeCurrent - averageCurrent);
  }

  // Serial diagnostics
  Serial.print(F("Voltage: "));
  Serial.print(voltage, 2);

  Serial.print(F(" V | Current: "));
  Serial.print(current, 3);

  Serial.print(F(" A | Used: "));
  Serial.print(usedAh, 4);

  Serial.print(F(" Ah | Remaining: "));
  Serial.print(batteryPercent, 1);

  Serial.println(F("%"));
}

// =====================================================
// Time formatting
// =====================================================

void formatTime(
    unsigned long totalSeconds,
    char* buffer,
    size_t bufferSize) {

  unsigned long hours = totalSeconds / 3600;
  unsigned int minutes = (totalSeconds % 3600) / 60;
  unsigned int seconds = totalSeconds % 60;

  snprintf(
    buffer,
    bufferSize,
    "%02lu:%02u:%02u",
    hours,
    minutes,
    seconds
  );
}

// =====================================================
// Page 1: Live measurements
// =====================================================

void drawLivePage() {
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(0, 9, "BATTERY MONITOR");

  u8g2.setCursor(0, 22);
  u8g2.print(F("Voltage: "));
  u8g2.print(voltage, 2);
  u8g2.print(F(" V"));

  u8g2.setCursor(0, 33);
  u8g2.print(F("Current: "));
  u8g2.print(current, 2);
  u8g2.print(F(" A"));

  u8g2.setCursor(0, 44);
  u8g2.print(F("Power:   "));
  u8g2.print(power, 2);
  u8g2.print(F(" W"));

  char timeString[16];

  formatTime(
    (millis() - startTime) / 1000,
    timeString,
    sizeof(timeString)
  );

  u8g2.setCursor(0, 61);
  u8g2.print(F("Up: "));
  u8g2.print(timeString);
}

// =====================================================
// Page 2: Battery capacity
// =====================================================

void drawCapacityPage() {
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(0, 9, "BATTERY CAPACITY");

  // Large battery percentage
  u8g2.setFont(u8g2_font_ncenB14_tr);

  u8g2.setCursor(0, 29);
  u8g2.print(batteryPercent, 0);
  u8g2.print(F("%"));

  // Battery progress bar
  u8g2.drawFrame(0, 34, 128, 10);

  int barWidth =
      (int)((batteryPercent / 100.0f) * 124.0f);

  barWidth = constrain(barWidth, 0, 124);

  if (barWidth > 0) {
    u8g2.drawBox(2, 36, barWidth, 6);
  }

  u8g2.setFont(u8g2_font_6x10_tr);

  // Amp-hours consumed and remaining
  u8g2.setCursor(0, 53);
  u8g2.print(F("Used: "));
  u8g2.print(usedAh, 2);
  u8g2.print(F(" Ah"));

  u8g2.setCursor(0, 63);
  u8g2.print(F("Left: "));
  u8g2.print(remainingAh, 2);
  u8g2.print(F(" Ah"));

  // Runtime estimate is available in Serial Monitor.
  // The 128x64 display is too small to show every
  // metric clearly on this page.
}

// =====================================================
// Display
// =====================================================

void drawResetPage() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 16);
  unsigned long held = millis() - buttonPressedAt;
  if (resetNotice) {
    u8g2.print(F("METRICS RESET"));
    u8g2.setCursor(0, 32);
    if (!persistence::available) u8g2.print(F("FRAM unavailable"));
    else if (resetSavePending) u8g2.print(F("FRAM save pending"));
    else u8g2.print(F("FRAM saved"));
  } else {
    // Blink the heading on each 500 ms refresh while the hold counts down.
    if ((held / 500) % 2 == 0) u8g2.print(F("HOLD TO RESET"));
    u8g2.setCursor(0, 32);
    u8g2.print(F("Reset in: "));
    u8g2.print(held >= RESET_HOLD_MS ? 0UL : (RESET_HOLD_MS - held + 999) / 1000);
    u8g2.print(F(" s"));
    u8g2.setCursor(0, 47);
    u8g2.print(F("Release to cancel"));
  }
  u8g2.setCursor(0, 62);
  u8g2.print(F("Assumes full battery"));
}

void drawFramDebugPage() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 9);
  u8g2.print(F("FRAM DEBUG"));
  u8g2.setCursor(0, 20);
  switch (persistence::status) {
    case persistence::Status::NotStarted: u8g2.print(F("Not started")); break;
    case persistence::Status::InitFailed: u8g2.print(F("INIT FAILED")); break;
    case persistence::Status::ReadFailed: u8g2.print(F("READ FAILED")); break;
    case persistence::Status::Restored: u8g2.print(F("Restored / ready")); break;
    case persistence::Status::Saved: u8g2.print(F("Save verified")); break;
    case persistence::Status::SaveFailed: u8g2.print(F("SAVE FAILED / retry")); break;
    case persistence::Status::InvalidCounters: u8g2.print(F("Invalid counters")); break;
  }
  u8g2.setCursor(0, 31);
  u8g2.print(F("ID: "));
  if (persistence::deviceIDRead) {
    u8g2.print(persistence::manufacturerID, HEX);
    u8g2.print(F(" / "));
    u8g2.print(persistence::productID, HEX);
  } else {
    u8g2.print(F("read failed"));
  }
  u8g2.setCursor(0, 42);
  u8g2.print(F("Slot: "));
  bool hasRecord = persistence::saved.magic == persistence::MAGIC;
  if (hasRecord) u8g2.print(persistence::activeSlot);
  else u8g2.print(F("none"));
  u8g2.setCursor(0, 53);
  u8g2.print(F("Seq: "));
  if (hasRecord) u8g2.print(persistence::saved.sequence);
  else u8g2.print(F("none"));
  u8g2.setCursor(0, 63);
  u8g2.print(F("Saved: "));
  if (persistence::hasWritten) {
    u8g2.print((millis() - persistence::lastSuccessfulSave) / 1000);
    u8g2.print(F("s ago"));
  } else {
    u8g2.print(F("not this boot"));
  }
}

void drawDisplay() {
  u8g2.firstPage();

  do {
    if (resetNotice || (stableButtonState == LOW && !resetHandled &&
                        millis() - buttonPressedAt >= 1000)) {
      drawResetPage();
    } else if (enableFramDebugPage && currentPage == 2) {
      drawFramDebugPage();
    } else if (!sensorAvailable) {
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(0, 20, "INA228 NOT FOUND");
    } else if (currentPage == 1) {
      drawCapacityPage();
    } else {
      drawLivePage();
    }

  } while (u8g2.nextPage());
}

// =====================================================
// Main loop
// =====================================================

void loop() {
  handleButton();

  unsigned long now = millis();
  unsigned long elapsed = now - lastOperatingUpdate;
  lastOperatingUpdate = now;
  uint32_t seconds = elapsed / 1000;
  uint16_t remainder = operatingRemainderMs + elapsed % 1000;
  seconds += remainder / 1000;
  operatingRemainderMs = remainder % 1000;
  // Saturate after about 136 years rather than wrapping the lifetime counter.
  operatingSeconds += min(seconds, UINT32_MAX - operatingSeconds);

  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;

    updateMeasurements(now);
    drawDisplay();
  }
  persistence::update(now, usedAh, usedWh, operatingSeconds);
  if (resetSavePending && persistence::status == persistence::Status::Saved) {
    resetSavePending = false;
  }
}
