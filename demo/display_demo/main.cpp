// ============================================================
//  BloomBot DISPLAY DEMO - only the screen + Wi-Fi / OTA / telnet.
//  No pump, no sensors, no server: nothing is sent to Telegram.
//
//  Built by env:display_demo / env:display_demo_ota (platformio.ini) from the
//  real src/display.cpp, logger.cpp, wifi_ota.cpp and state.cpp plus this file.
//  So a change in src/display.cpp is at once in the demo AND in the firmware.
//
//  Commands (Serial monitor or telnet bloombot.local:23):
//    0..6  - flower mood: 0=idle 1=need water 2=watering 3=low water
//            4=connecting 5=okay 6=error
//    d     - demo tour on/off: changes the mood every DEMO_STEP_MS
//    + -   - soil index +10 / -10
//    ] [   - tank level +10 / -10 %
//    x     - next error text (none, tank sensor, soil sensor, pump, still dry)
//    f     - frame time (last / max) and free memory
//    i     - Wi-Fi info
//    h     - this help
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "logger.h"
#include "wifi_ota.h"
#include "display.h"

#define DEMO_STEP_MS  6000UL   // demo tour: time per mood

// wifi_ota.cpp calls pumpLock() when an OTA upload starts. There is no pump here.
void pumpLock() {}

static const char* STATUS_NAMES[] = {
  "IDLE", "NEED_WATER", "WATERING", "LOW_WATER", "CONNECTING", "OKAY", "ERROR"
};  // same order as enum LedStatus in state.h

// Demo tour order: the moods a visitor would see
static const LedStatus TOUR[] = {
  STATUS_OKAY, STATUS_NEED_WATER, STATUS_WATERING, STATUS_OKAY, STATUS_LOW_WATER, STATUS_ERROR
};
static const int TOUR_LEN = sizeof(TOUR) / sizeof(TOUR[0]);
static bool tourOn = true;            // start with the tour, so it moves without a laptop
static int tourPos = 0;
static unsigned long tourStepAt = 0;


static int errorPos = 0;   // 0 = no error, see setError()

// The real firmware shows LOW_WATER only below TANK_LOW_ON_PERCENT.
// The demo does the same: the tank drops to 12 % and comes back afterwards.
#define DEMO_LOW_TANK_PERCENT  12
static int tankBeforeLow = -2;   // -2 = not in LOW_WATER

static void setStatus(LedStatus s) {
  if (s == STATUS_LOW_WATER && tankBeforeLow == -2) {
    tankBeforeLow = state.tankPercent;
    state.tankPercent = DEMO_LOW_TANK_PERCENT;
  } else if (s != STATUS_LOW_WATER && tankBeforeLow != -2) {
    state.tankPercent = tankBeforeLow;
    tankBeforeLow = -2;
  }
  state.ledStatus = s;
  Log.print("-> mood: ");
  Log.println(STATUS_NAMES[s]);
}

// ONE demo plant with real data: Fensterblatt (Monstera), config.h DEMO_* (same as src/sim.cpp)
static void setDemoPlant() {
  state.profile.plantId = DEMO_PLANT_ID;
  strncpy(state.profile.name, DEMO_PLANT_NAME, sizeof(state.profile.name) - 1);
  state.profile.name[sizeof(state.profile.name) - 1] = '\0';
  state.profile.moistMin = DEMO_MOIST_MIN;
  state.profile.moistTarget = DEMO_MOIST_TARGET;
  state.profile.moistMax = DEMO_MOIST_MAX;
  state.profile.doseMl = DEMO_DOSE_ML;
  state.profile.minPauseH = DEMO_MIN_PAUSE_H;
  Log.printf("-> plant: %s, soil %d/%d/%d, portion %d ml\r\n", DEMO_PLANT_NAME,
             DEMO_MOIST_MIN, DEMO_MOIST_TARGET, DEMO_MOIST_MAX, DEMO_DOSE_ML);
}

// The same flags the real modules set; display.cpp turns them into the error text
static void setError(int i) {
  state.tankSensorFail = (i == 1);
  state.moistureSensorFail = (i == 2);
  state.errorReason = (i == 3) ? "pump_timeout" : (i == 4) ? "still_dry" : nullptr;
  static const char* names[] = { "none", "tank sensor", "soil sensor", "pump_timeout", "still_dry" };
  Log.print("-> error: ");
  Log.println(names[i]);
}

static void printHelp() {
  Log.println("DEMO commands: 0-6 mood, d tour on/off, +/- soil, ]/[ tank, x error, f frame/memory, i wifi, h help");
  Log.println("Web page keys (same as the real firmware, played as moods): D w L V r o a m s t j");
}

// Status line for the web page (scripts/device-bridge.mjs), same format as src/sim.cpp
static void printState() {
  const char* err = state.errorReason ? state.errorReason
                  : state.tankSensorFail ? "sensor_tank"
                  : state.moistureSensorFail ? "sensor_moisture" : "";
  Log.printf("STATE {\"fw\":\"demo\",\"st\":%d,\"ph\":\"DEMO\",\"soil\":%d,\"tank\":%d,\"need\":%d,"
             "\"att\":0,\"err\":\"%s\",\"last\":0,\"total\":0,\"wifi\":%d,"
             "\"sim\":{\"soil\":%d,\"tank\":%d,\"hose\":1},\"min\":%d,\"tgt\":%d,\"max\":%d,\"tour\":%d}\r\n",
             (int)state.ledStatus, state.moistureIndex, state.tankPercent,
             state.ledStatus == STATUS_NEED_WATER ? 1 : 0, err, state.wifiConnected ? 1 : 0,
             state.moistureIndex, state.tankPercent,
             DEMO_MOIST_MIN, DEMO_MOIST_TARGET, DEMO_MOIST_MAX, tourOn ? 1 : 0);
}

// Keys of the real firmware (src/main.cpp, src/sim.h), played as moods: no pump here
static bool handleWebKey(char ch) {
  switch (ch) {
    case 'j': printState(); return true;
    case 'D': tourOn = false; state.moistureIndex = DEMO_MOIST_MIN - 10; setStatus(STATUS_NEED_WATER); return true;
    case 'w': tourOn = false; setStatus(STATUS_WATERING); return true;
    case 'L': tourOn = false; setStatus(STATUS_LOW_WATER); return true;
    case 'V': tourOn = false; setStatus(STATUS_OKAY); state.tankPercent = 100; return true;
    case 'r':
    case 'o': tourOn = false; errorPos = 0; setError(0); state.moistureIndex = DEMO_MOIST_TARGET; setStatus(STATUS_OKAY); return true;
    case 'a': tourOn = true; tourStepAt = millis(); Log.println("-> tour ON"); return true;
    case 'm':
    case 's':
    case 't': Log.printf("DEMO: soil %d, tank %d %% (no sensors, no pump)\r\n", state.moistureIndex, state.tankPercent); return true;
    default:  return false;
  }
}

static void handleCommand(char ch) {
  if (handleWebKey(ch)) return;
  if (ch >= '0' && ch <= '6') {
    tourOn = false;                       // manual choice stops the tour
    setStatus((LedStatus)(ch - '0'));
    return;
  }
  switch (ch) {
    case 'd':
      tourOn = !tourOn;
      tourStepAt = millis();
      Log.println(tourOn ? "-> tour ON" : "-> tour OFF");
      break;
    case '+': state.moistureIndex = min(state.moistureIndex + 10, 100); Log.printf("-> soil %d\r\n", state.moistureIndex); break;
    case '-': state.moistureIndex = max(state.moistureIndex - 10, -1);  Log.printf("-> soil %d\r\n", state.moistureIndex); break;
    case ']': state.tankPercent = min(state.tankPercent + 10, 100);     Log.printf("-> tank %d\r\n", state.tankPercent); break;
    case '[': state.tankPercent = max(state.tankPercent - 10, -1);      Log.printf("-> tank %d\r\n", state.tankPercent); break;
    case 'x': errorPos = (errorPos + 1) % 5; setError(errorPos); break;
    case 'f': displayPrintStats(); break;
    case 'i': wifiOtaPrintInfo(); break;
    case 'h': printHelp(); break;
    default: break;   // newline characters etc.
  }
}

void setup() {
  Serial.begin(115200);

  // Fake data for the data line (-1 would show "--")
  state.moistureIndex = DEMO_MOIST_TARGET;
  state.tankPercent = 68;
  setDemoPlant();
  setStatus(TOUR[0]);

  displayBegin();
  wifiOtaBegin();
  Log.println("BloomBot DISPLAY DEMO running.");
  printHelp();
}

void loop() {
  wifiOtaUpdate();      // keep first: OTA must be handled often
  loggerUpdate();

  if (tourOn && millis() - tourStepAt >= DEMO_STEP_MS) {
    tourStepAt = millis();
    tourPos = (tourPos + 1) % TOUR_LEN;
    setStatus(TOUR[tourPos]);
  }

  displayUpdate();

  while (Serial.available() > 0) handleCommand(Serial.read());
  int c;
  while ((c = loggerReadChar()) >= 0) handleCommand((char)c);
}
