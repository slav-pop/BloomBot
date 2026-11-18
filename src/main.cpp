#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "logger.h"
#include "events.h"
#include "net.h"
#include "leds.h"
#include "water_tank.h"
#include "moisture.h"
#include "pump.h"
#include "wifi_ota.h"
#include "profile.h"

// ============================================================
//  Commands - in the Serial Monitor (cable) or over Wi-Fi (telnet, see logger.h):
//    s     - watering status
//    t     - tank data
//    m     - measure soil now
//    w     - water now (manual test: checks the tank, ignores moist_max and the pause)
//    r     - reset watering error
//    i     - Wi-Fi: IP address and signal, server connection
//    n     - send the status to the server now
//    p     - show the plant profile
//    e     - show the events waiting for the server
//    F     - forget the plant profile (factory reset), back to defaults
//    T     - apply a test profile (Monstera) and save it in flash
//    a     - LED: automatic status (normal mode)
//    0..6  - LED: force a status (test):
//            0=rainbow 1=need water 2=watering 3=low water
//            4=connecting 5=okay 6=error
// ============================================================

static bool manualMode = false;  // true = manual LED choice from keyboard

// serial 'm': the measurement is non-blocking, print when the result is there
static bool printMoistureWhenReady = false;
static uint32_t printMoistureSeq = 0;

static void printMoisture() {
  Log.print("MOISTURE: sensor1 raw=");
  Log.print(state.moistureRaw[0]);
  Log.print(" (");
  Log.print(state.moistureSensorIndex[0]);
  Log.print("), sensor2 raw=");
  Log.print(state.moistureRaw[1]);
  Log.print(" (");
  Log.print(state.moistureSensorIndex[1]);
  Log.print(") -> ");
  Log.print(state.moistureIndex);
  Log.print(state.moistureSensorFail ? "/100 BOTH FAILED" : state.moistureMismatch ? "/100 MISMATCH" : "/100");
  Log.print(", ledStatus=");
  Log.println(state.ledStatus);
}

// ---- Core: choose what is more important to show ----
// error > watering > low/empty tank > dry soil > connecting > okay > idle
static void updateStatus() {
  if (manualMode) return;

  if (state.errorFlag || state.tankSensorFail || state.moistureSensorFail) {
    state.ledStatus = STATUS_ERROR;               // the error overrides everything else
  } else if (state.pumpOn) {
    state.ledStatus = STATUS_WATERING;
  } else if (state.tankLow || state.waterPhase == PHASE_TANK_EMPTY) {
    state.ledStatus = STATUS_LOW_WATER;           // user action: refill the tank
  } else if (state.needWater) {
    state.ledStatus = STATUS_NEED_WATER;
  } else if (state.wifiConnecting) {
    state.ledStatus = STATUS_WIFI_CONNECTING;
  } else if (state.moistureIndex >= 0) {
    state.ledStatus = STATUS_OKAY;                // sensor data present, all fine
  } else {
    state.ledStatus = STATUS_IDLE;                // no data yet
  }
}

static void setManualLed(LedStatus s, const char* name) {
  manualMode = true;
  state.ledStatus = s;
  Log.print("-> LED manual: ");
  Log.println(name);
}

// ---- Test control: one command character, from the cable or from telnet ----
static void handleCommand(char ch) {
  switch (ch) {
  case 's':
    Log.print("WATER: phase=");
    Log.print(waterPhaseName(state.waterPhase));
    Log.print(", needWater=");
    Log.print(state.needWater ? "YES" : "NO");
    Log.print(", pumpOn=");
    Log.print(state.pumpOn ? "YES" : "NO");
    Log.print(", attempts=");
    Log.print(state.waterAttempts);
    Log.print(", error=");
    Log.println(state.errorReason ? state.errorReason : "NO");
    Log.print("WATER: total ");
    Log.print(state.totalPumpedMl);
    Log.print(" ml since boot, last run ");
    Log.print(state.lastDoseMl);
    Log.print(" ml");
    if (state.wateredSinceBoot) {
      Log.print(", ");
      Log.print((millis() - state.lastWateredAt) / 1000UL);
      Log.println(" s ago");
    } else {
      Log.println(", not watered since boot");
    }
    break;
  case 't':
    Log.print("TANK: ");
    if (state.tankPercent < 0) {
      Log.println(state.tankSensorFail ? "SENSOR FAILED (no echo)" : "no data");
    } else {
      Log.print(state.tankDistanceCm, 1);
      Log.print(" cm to water, level ");
      Log.print(state.tankPercent);
      Log.print(" %, tankLow=");
      Log.println(state.tankLow ? "YES" : "NO");
    }
    break;
  case 'm':
    printMoistureSeq = state.moistureSeq;
    printMoistureWhenReady = true;
    moistureRequest();
    break;
  case 'w':
    state.waterRequest = true;
    Log.println("-> water now (only in IDLE)");
    break;
  case 'r':
    state.errorResetRequest = true;
    Log.println("-> reset error (only in ERROR)");
    break;
  case 'i':
    wifiOtaPrintInfo();
    netPrintInfo();
    break;
  case 'n':
    netPollNow();
    Log.println("-> send status to the server now");
    break;
  case 'p':
    profilePrint();
    break;
  case 'e':
    eventPrintAll();
    break;
  case 'F':
    profileFactoryReset();   // same as the server command "factory_reset"
    break;
  case 'T': {
    // test profile as the server would send it (Monstera, 2 l pot),
    // with a new version so it really counts as "new"
    Profile t;
    profileDefaults(t);
    t.ver = state.profile.ver + 1;
    t.plantId = 9001;
    strncpy(t.name, "Swiss cheese plant", sizeof(t.name) - 1);
    t.moistMin = 30;
    t.moistTarget = 50;
    t.moistMax = 70;
    t.doseMl = 150;
    t.minPauseH = 84;
    profileApply(t, nullptr);
    profilePrint();
    break;
  }
  case 'a':
    manualMode = false;
    Log.println("-> LED auto mode");
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
  profileBegin();       // before the first soil check
  wifiOtaBegin();        // IP address is printed as soon as Wi-Fi is connected

  Log.println("The system is running. Commands: s t m w r i n p T e F a 0-6");
  eventPush("boot", 0, 0, FW_VERSION);
#if WATER_TEST_TIMING
  Log.println("!!! WATER_TEST_TIMING = 1 (short test times) !!!");
#endif
}

void loop() {
  wifiOtaUpdate();      // keep first: OTA must be handled often
  loggerUpdate();       // telnet: start server, accept client, send held-back text
  netUpdate();          // server: status every poll_s, events at once (never while pumping)
  tankUpdate();
  moistureUpdate();     // before pumpUpdate: a finished result is used in the same loop
  pumpUpdate();
  updateStatus();
  ledsUpdate();
  // commands: from the USB cable and from the telnet client
  while (Serial.available() > 0) handleCommand(Serial.read());
  int c;
  while ((c = loggerReadChar()) >= 0) handleCommand((char)c);

  if (printMoistureWhenReady && state.moistureSeq != printMoistureSeq) {
    printMoistureWhenReady = false;
    printMoisture();
  }
}