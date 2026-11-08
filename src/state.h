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
  STATUS_OKAY,            // green breathing
  STATUS_ERROR            // critical error
};

// Phases of the watering state machine (pump.cpp)
enum WaterPhase {
  PHASE_IDLE,        // waiting for the next soil check
  PHASE_PUMPING,     // pump is on
  PHASE_SOAKING,     // pump off, water soaks into the soil
  PHASE_TANK_EMPTY,  // soil is dry, but no water in the tank
  PHASE_ERROR        // still dry after all attempts, manual reset needed
};

struct SystemState {
  // ---- tank (from water_tank.cpp) ----
  float tankDistanceCm = -1;   // distance to water;  -1 = no data/error
  int tankPercent = -1;        // water level 0..100; -1 = no data
  bool tankLow = false;

  // ---- soil moisture (moisture.cpp) ----
  int moistureRaw = -1;        // ADC 0..4095; -1 = no data
  int moisturePercent = -1;    // 0..100;      -1 = no data

  // ---- watering (pump.cpp is the ONLY module that writes these) ----
  WaterPhase waterPhase = PHASE_IDLE;
  bool needWater = false;
  bool pumpOn = false;
  uint8_t waterAttempts = 0;

  // ---- requests from the user (serial now, web/bot later) ----
  // main.cpp sets them to true, pump.cpp handles and clears them
  bool waterRequest = false;       // "water now"
  bool errorResetRequest = false;  // "reset the error"

  // ---- network (wifi_ota.cpp is the ONLY module that writes these) ----
  bool wifiConnected = false;
  bool wifiConnecting = false;

  // --- critical error (can be triggered by any module) ---
  bool errorFlag = false;

  // ---- final state (write to main.cpp) ----
  LedStatus ledStatus = STATUS_IDLE; 
};

extern SystemState state;
