#pragma once
#include <Arduino.h>

// ============================================
// Common State
// ============================================

// All LED-status
enum LedStatus {
  STATUS_IDLE,            // all is OK / test-mode
  STATUS_NEED_WATER,      // red breathing
  STATUS_WATERING,        // blue comet
  STATUS_LOW_WATER,       // yellow warning
  STATUS_WIFI_CONNECTING, // runnig stripe
  STATUS_WIFI_CONNECTED,  // white
  STATUS_OKAY,            // green breathing
  STATUS_ERROR            // critical error
};

struct SystemState {
  // ---- tank (from water_tank.cpp) ----
  float tankDistanceCm = -1;   // distance to water;  -1 = no data/error
  int tankPercent = -1;        // water level 0..100; -1 = no data
  bool tankLow = false;

  // ---- soil moisture (from Tobias) ----
  int moistureRaw = -1;
  bool needWater = false;

  // ---- pump (from Tobias) ----
  // bool plantGetsWater = false;  // Testing if water reaches plant, so that the tube is in the plant pot and not on the ground
  bool pumpOn = false;

  // ---- network (from Telegram - Luca) ----
  bool wifiConnected = false;
  bool wifiConnecting = false;

  // --- critical error (can be triggered by any module) ---
  bool errorFlag = false;

  // ---- final state (write to main.cpp) ----
  LedStatus ledStatus = STATUS_IDLE; 
};

extern SystemState state;
