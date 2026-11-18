#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "logger.h"
#include "pump.h"
#include "sim.h"

// ============================================================
//  sim.cpp - see sim.h. Only moisture.cpp and water_tank.cpp ask for the
//  simulated values; the pump never knows that it is a demo.
// ============================================================

static const int SIM_STEP          = 10;     // + - [ ]
static const int SIM_DRY_BELOW_MIN = 10;     // 'D': this much below the profile's moist_min
static const uint32_t SIM_GLASS_BUDGET_ML = 300;  // the glass at the fair; more = "tank empty"
static const int SIM_LOW_TANK      = 15;     // 'L': below TANK_LOW_ON_PERCENT (20)
static const int SIM_FULL_TANK     = 100;    // 'V'
static const int SIM_UNKNOWN_SOIL  = 50;     // start value if the real sensors have no data
static const float SIM_SOIL_PER_ML = 0.25f;  // a portion makes the soil wetter by ml x this ...
static const int SIM_MIN_SOIL_GAIN = 12;     // ... but at least this (dry -> target in <= 3 portions)

static int   soil = -1;          // -1 = real sensors
static float tank = -1;          // -1 = real sensor only
static bool  hoseOff = false;    // true: pumped water does not reach the soil
static bool  wasPumping = false;
static uint32_t glassStartMl = 0;   // state.totalPumpedMl when the glass was emptied ('G')
static bool  glassFullLogged = false;

static int clampIndex(int v) { return constrain(v, 0, 100); }

static bool simActive() { return soil >= 0 || tank >= 0; }

static uint32_t glassMl() { return state.totalPumpedMl - glassStartMl; }

// Glass budget: while the demo runs, the hose ends in a glass, not in a pot.
// Visitors may press "water now" again and again - after SIM_GLASS_BUDGET_ML the
// tank counts as empty (pump.cpp -> TANK_EMPTY). Only 'G' (terminal, not the web
// page) and a reboot start a new glass; 'o' does not, so the limit can not be skipped.
static bool glassFull() { return simActive() && glassMl() >= SIM_GLASS_BUDGET_ML; }

int simSoil() { return soil; }

int simTankLimit() {
  if (glassFull()) {
    if (!glassFullLogged) {
      Log.printf("SIM: glass full (%lu ml) - empty it, then type G in the terminal\r\n",
                 (unsigned long)glassMl());
      glassFullLogged = true;
    }
    return 0;
  }
  return tank < 0 ? -1 : (int)(tank + 0.5f);
}

// The screen and the web page show the new value at once; the pump uses it
// at its next soil check (moisture.cpp asks simSoil() after every measurement)
static void setSoil(int v) {
  soil = clampIndex(v);
  state.moistureIndex = soil;
  state.moistureSensorFail = false;   // simulated soil hides a broken real sensor (by design)
  state.moistureSensorIndex[0] = soil;
  state.moistureSensorIndex[1] = soil;
  state.moistureMismatch = false;
  Log.printf("SIM: soil index %d\r\n", soil);
}

// Used by water_tank.cpp at its next reading (every 2 s, faster while pumping)
static void setTank(float v) {
  tank = constrain(v, 0.0f, 100.0f);
  Log.printf("SIM: tank %d %% (never more than the real tank)\r\n", (int)(tank + 0.5f));
}

static int soilNow() {
  if (soil >= 0) return soil;
  return state.moistureIndex >= 0 ? state.moistureIndex : SIM_UNKNOWN_SOIL;
}

static float tankNow() {
  if (tank >= 0) return tank;
  return state.tankPercent >= 0 ? state.tankPercent : SIM_FULL_TANK;
}

static void simOff() {
  soil = -1;
  tank = -1;
  hoseOff = false;
  Log.println("SIM: off - real sensors again (values update with the next measurement)");
}

// One line for the web page, plain JSON after "STATE ".
// Not while pumping: the logger holds telnet text back then (pump safety),
// a STATE line every second would overflow its buffer.
static void printState() {
  if (state.pumpOn) return;
  Log.printf("STATE {\"fw\":\"main\",\"st\":%d,\"ph\":\"%s\",\"soil\":%d,\"tank\":%d,\"need\":%d,\"att\":%d,"
             "\"err\":\"%s\",\"last\":%u,\"total\":%lu,\"wifi\":%d,"
             "\"sim\":{\"soil\":%d,\"tank\":%d,\"hose\":%d},\"min\":%d,\"tgt\":%d,\"max\":%d,"
             "\"glass\":%lu,\"glassMax\":%lu}\r\n",
             (int)state.ledStatus, waterPhaseName(state.waterPhase), state.moistureIndex,
             state.tankPercent, state.needWater ? 1 : 0, state.waterAttempts,
             state.errorReason ? state.errorReason : "", (unsigned)state.lastDoseMl,
             (unsigned long)state.totalPumpedMl, state.wifiConnected ? 1 : 0,
             soil, simTankLimit(), hoseOff ? 0 : 1,
             state.profile.moistMin, state.profile.moistTarget, state.profile.moistMax,
             (unsigned long)(simActive() ? glassMl() : 0), (unsigned long)SIM_GLASS_BUDGET_ML);
}

bool simHandleCommand(char c) {
  switch (c) {
    case '+': setSoil(soilNow() + SIM_STEP); return true;
    case '-': setSoil(soilNow() - SIM_STEP); return true;
    case 'D': setSoil(state.profile.moistMin - SIM_DRY_BELOW_MIN); return true;
    case ']': setTank(tankNow() + SIM_STEP); return true;
    case '[': setTank(tankNow() - SIM_STEP); return true;
    case 'L': setTank(SIM_LOW_TANK); return true;
    case 'V': setTank(SIM_FULL_TANK); return true;
    case 'H':
      hoseOff = !hoseOff;
      Log.println(hoseOff ? "SIM: hose OFF - the water does not reach the soil"
                          : "SIM: hose ON again");
      return true;
    case 'o': simOff(); return true;
    case 'G':                              // terminal only (not in the web page's key list)
      glassStartMl = state.totalPumpedMl;
      glassFullLogged = false;
      Log.println("SIM: new empty glass, budget starts again");
      return true;
    case 'j': printState(); return true;
    default:  return false;
  }
}

// After every pump run: the simulated soil gets wetter (unless the hose is off)
// and the simulated tank loses the pumped water
void simUpdate() {
  if (state.pumpOn) {
    wasPumping = true;
    return;
  }
  if (!wasPumping) return;
  wasPumping = false;

  uint16_t ml = state.lastDoseMl;
  if (tank >= 0) setTank(tank - ml * 100.0f / TANK_CAPACITY_ML);
  if (soil >= 0 && !hoseOff) {
    setSoil(soil + max(SIM_MIN_SOIL_GAIN, (int)(ml * SIM_SOIL_PER_ML + 0.5f)));
  }
}
