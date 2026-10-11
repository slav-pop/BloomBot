#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "leds.h"
#include "water_tank.h"
#include "moisture.h"
#include "pump.h"

// ============================================================
//  Commands in the Serial Monitor:
//    s     - watering status
//    t     - tank data
//    m     - measure soil now
//    w     - water now (goes through all safety checks)
//    r     - reset watering error
//    a     - LED: automatic status (normal mode)
//    0..6  - LED: force a status (test):
//            0=rainbow 1=need water 2=watering 3=low water
//            4=connecting 5=okay 6=error
// ============================================================

static bool manualMode = false;  // true = manual LED choice from keyboard

// ---- Core: choose what is more important to show ----
// error > watering > low/empty tank > dry soil > connecting > okay > idle
static void updateStatus() {
  if (manualMode) return;

  if (state.errorFlag) {
    state.ledStatus = STATUS_ERROR;               // the error overrides everything else
  } else if (state.pumpOn) {
    state.ledStatus = STATUS_WATERING;
  } else if (state.tankLow || state.waterPhase == PHASE_TANK_EMPTY) {
    state.ledStatus = STATUS_LOW_WATER;           // user action: refill the tank
  } else if (state.needWater) {
    state.ledStatus = STATUS_NEED_WATER;
  } else if (state.wifiConnecting) {
    state.ledStatus = STATUS_WIFI_CONNECTING;
  } else if (state.moistureRaw >= 0) {
    state.ledStatus = STATUS_OKAY;                // sensor data present, all fine
  } else {
    state.ledStatus = STATUS_IDLE;                // no data yet
  }
}

static void setManualLed(LedStatus s, const char* name) {
  manualMode = true;
  state.ledStatus = s;
  Serial.print("-> LED manual: ");
  Serial.println(name);
}

// ---- Test Control via Serial ----
static void handleSerial() {
  if (Serial.available() == 0) return;
  char ch = Serial.read();

  switch (ch) {
  case 's':
    Serial.print("WATER: phase=");
    Serial.print(waterPhaseName(state.waterPhase));
    Serial.print(", needWater=");
    Serial.print(state.needWater ? "YES" : "NO");
    Serial.print(", pumpOn=");
    Serial.print(state.pumpOn ? "YES" : "NO");
    Serial.print(", attempts=");
    Serial.print(state.waterAttempts);
    Serial.print(", error=");
    Serial.println(state.errorFlag ? "YES" : "NO");
    break;
  case 't':
    Serial.print("TANK: ");
    if (state.tankPercent < 0) {
      Serial.println("no data");
    } else {
      Serial.print(state.tankDistanceCm, 1);
      Serial.print(" cm to water, level ");
      Serial.print(state.tankPercent);
      Serial.print(" %, tankLow=");
      Serial.println(state.tankLow ? "YES" : "NO");
    }
    break;
  case 'm':
    moistureMeasure();
    Serial.print("MOISTURE: raw=");
    Serial.print(state.moistureRaw);
    Serial.print(", ");
    Serial.print(state.moisturePercent);
    Serial.print(" %, ledStatus=");
    Serial.println(state.ledStatus);
    break;
  case 'w':
    state.waterRequest = true;
    Serial.println("-> water now (only in IDLE)");
    break;
  case 'r':
    state.errorResetRequest = true;
    Serial.println("-> reset error (only in ERROR)");
    break;
  case 'a':
    manualMode = false;
    Serial.println("-> LED auto mode");
    break;
  case '0': setManualLed(STATUS_IDLE, "IDLE (rainbow)"); break;
  case '1': setManualLed(STATUS_NEED_WATER, "NEED_WATER"); break;
  case '2': setManualLed(STATUS_WATERING, "WATERING"); break;
  case '3': setManualLed(STATUS_LOW_WATER, "LOW_WATER"); break;
  case '4': setManualLed(STATUS_WIFI_CONNECTING, "CONNECTING"); break;
  case '5': setManualLed(STATUS_OKAY, "OKAY"); break;
  case '6': setManualLed(STATUS_ERROR, "ERROR"); break;
  default: break;   // ignore newline characters etc.
  }
}

void setup() {
  pumpBegin();          // FIRST: relay off as early as possible

  Serial.begin(115200);

  ledsBegin();
  tankBegin();
  moistureBegin();

  Serial.println("The system is running. Commands: s t m w r a 0-6");
#if WATER_TEST_TIMING
  Serial.println("!!! WATER_TEST_TIMING = 1 (short test times) !!!");
#endif
}

void loop() {
  tankUpdate();
  pumpUpdate();
  updateStatus();
  ledsUpdate();
  handleSerial();
}