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
#include "display.h"
#include "sim.h"

// ============================================================
//  Commands - in the Serial Monitor (cable) or over Wi-Fi (telnet, see logger.h).
//  'h' prints this list on the device (printHelp, keep both in sync):
//    h, ?  - help: all commands with a short description
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
//    f     - screen: frame time (last / max) and free memory
//    Fair demo (sim.h): - + D soil, [ ] L V tank, H hose, o real sensors, j status line
//    a     - LED: automatic status (normal mode)
//    0..6  - LED + screen: force a status (test):
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
// error > watering > low/empty tank > dry soil > connecting > no plant (idle) > okay
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
  } else if (state.profile.plantId < 0 || state.moistureIndex < 0) {
    // No plant assigned yet (waters with the default profile) or no soil
    // data yet right after boot: rainbow = "choose a plant in the app"
    state.ledStatus = STATUS_IDLE;
  } else {
    state.ledStatus = STATUS_OKAY;                // plant assigned, sensor data present, all fine
  }
}

static void setManualLed(LedStatus s, const char* name) {
  manualMode = true;
  state.ledStatus = s;
  Log.print("-> LED manual: ");
  Log.println(name);
}

// ---- 'h' / '?': list of all commands, grouped, with a short description ----
static void printHelp() {
  Log.println();
  Log.println("================= BloomBot commands =================");
  Log.println("Type one character (Serial Monitor or telnet).");
  Log.println();
  Log.println("STATUS / INFO");
  Log.println("  s   watering status: phase, attempts, last watering");
  Log.println("  t   water tank: distance to water, level %");
  Log.println("  m   measure soil moisture now (both sensors)");
  Log.println("  i   Wi-Fi (IP, signal) and server connection");
  Log.println("  p   show the current plant profile");
  Log.println("  e   show events waiting to be sent to the server");
  Log.println("  f   screen: frame time and free memory");
  Log.println();
  Log.println("ACTIONS");
  Log.println("  w   water now (manual test; still checks the tank)");
  Log.println("  r   reset a watering error (only in phase ERROR)");
  Log.println("  n   send the status to the server now");
  Log.println();
  Log.println("PLANT PROFILE");
  Log.println("  T   load a test profile (Monstera), saved in flash");
  Log.println("  F   FACTORY RESET: forget the plant profile");
  Log.println();
  Log.println("LED STRIP + SCREEN TEST (force a status)");
  Log.println("  0 no plant (rainbow)  1 need water  2 watering  3 low water");
  Log.println("  4 Wi-Fi connecting  5 okay  6 error");
  Log.println("  a   back to automatic status (normal mode)");
  simPrintHelp();
  Log.println();
  Log.println("  h   show this help again");
  Log.println("=====================================================");
}

// ---- Test control: one command character, from the cable or from telnet ----
static void handleCommand(char ch) {
  if (simHandleCommand(ch)) return;   // fair demo commands, see sim.h
  switch (ch) {
  case 'h':
  case '?':
    printHelp();
    break;
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
  case 'f':
    displayPrintStats();
    break;
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
  displayBegin();       // after profileBegin: the screen shows the plant name
  wifiOtaBegin();        // IP address is printed as soon as Wi-Fi is connected

  Log.println("The system is running. Type 'h' for the list of commands.");
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
  simUpdate();          // fair demo: soil / tank follow the pumped water
  updateStatus();
  ledsUpdate();
  displayUpdate();      // after updateStatus: the flower shows the same status as the LEDs
  // commands: from the USB cable and from the telnet client
  while (Serial.available() > 0) handleCommand(Serial.read());
  int c;
  while ((c = loggerReadChar()) >= 0) handleCommand((char)c);

  if (printMoistureWhenReady && state.moistureSeq != printMoistureSeq) {
    printMoistureWhenReady = false;
    printMoisture();
  }
}