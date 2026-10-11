#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "leds.h"
#include "water_tank.h"
#include "moisture.h"
#include "pump.h"

// ============================================================
//  Test commands in the Serial Monitor:
//    a       — automatic status selection (operational mode)
//    0..5    — force a status (manual mode):
//              0=rainbow 1=needs watering 2=watering 3=low water
//              4=connecting to Wi-Fi 5=connected to Wi-Fi
//    t       — print tank data
// ============================================================

static bool manualMode = false;  // true = manual choice from keyboard

// ---- Core: choose, what is more important to show ----
// watering > dry soil > low water > connecting > connected

static void updateStatus() {
  if (manualMode) return;

  if (state.errorFlag) {
    state.ledStatus = STATUS_ERROR;               // the error overrides everything else
  } else if (state.pumpOn) {
    state.ledStatus = STATUS_WATERING;
  } else if (state.needWater) {
    state.ledStatus = STATUS_NEED_WATER;
  } else if (state.tankLow) {
    state.ledStatus = STATUS_LOW_WATER;
  } else if (state.wifiConnecting) {
    state.ledStatus = STATUS_WIFI_CONNECTING;
  } else if (state.wifiConnected) {
    state.ledStatus = STATUS_WIFI_CONNECTED;
  } else {
    state.ledStatus = STATUS_IDLE;
  }
}

// ---- Test Control via Serial ----
static void handleSerial() {
  if (Serial.available() == 0) return;
  char ch = Serial.read();

  switch (ch) {
  case 'a':
    manualMode = false;
    Serial.print("-> AUTO MODE: Status is determined based on sensor data");
    break;
  case '0':
    manualMode = true;
    state.ledStatus = STATUS_IDLE;
    Serial.print("-> Manual: IDLE (rainbow)");
    break;
  case '1':
    manualMode = true;
    state.ledStatus = STATUS_NEED_WATER;
    Serial.print("--> Manual: NEED_WATER (red breathing)");
    break;
  case '2':
    manualMode = true;
    state.ledStatus = STATUS_WATERING;
    Serial.print("-> Manual: WATERING (blue comet)");
    break;
  case '3':
    manualMode = true;
    state.ledStatus = STATUS_LOW_WATER;
    Serial.print("-> Manual: Low water in tank");
    break;
  case '4':
    manualMode = true;
    state.ledStatus = STATUS_WIFI_CONNECTING;
    Serial.print("-> Manual: Connecting");
    break;
  case '5':
    manualMode = true;
    state.ledStatus = STATUS_OKAY;
    Serial.print("-> Manual: All is fine");
    break;
  case '6':
    manualMode = true;
    state.ledStatus = STATUS_ERROR;
    Serial.print("-> Manual: ERROR");
    break;
  case 't':
    Serial.print("TANK: ");
    if (state.tankPercent < 0) {
      Serial.print("No data");
    } else {
      Serial.print(state.tankDistanceCm);
      Serial.print("distance to water, level");
      Serial.print(state.tankPercent);
      Serial.print("%, tankLow=");
      Serial.print(state.tankLow ? "YES" : "NO");
    }
    break;
  case 'p':
    state.pumpTestRequest = true;
    Serial.println("-> PUMP TEST requested");
    break;
  case 'm':
    Serial.print("MOISTURE: raw=");
    Serial.print(state.moistureRaw);
    Serial.print(", needWater=");
    Serial.println(state.needWater ? "YES" : "NO");
    break;
  }
}

void setup() {
  pumpBegin();          // FIRST: relay off as early as possible

  Serial.begin(115200);

  ledsBegin();
  tankBegin();
  moistureBegin();

  Serial.println("The system is running. Commands: a, 0-6, t, p, m");
}

void loop() {
  tankUpdate();
  moistureUpdate();
  pumpUpdate();
  updateStatus();
  ledsUpdate();
  handleSerial();
}
