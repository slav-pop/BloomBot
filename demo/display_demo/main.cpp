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
//    n     - next plant name (incl. no profile, umlaut and long name)
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

// "" = no profile -> screen shows "BloomBot". "\xC3\xBC" = UTF-8 "ü", as the server sends it
static const char* PLANT_NAMES[] = {
  "", "Monstera", "Gr\xC3\xBCnlilie", "Bogenhanf", "Geigenfeige (Ficus lyrata)"
};
static const int PLANT_COUNT = sizeof(PLANT_NAMES) / sizeof(PLANT_NAMES[0]);
static int plantPos = 1;

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

static void setPlant(int i) {
  const char* name = PLANT_NAMES[i];
  state.profile.plantId = name[0] ? 9000 + i : -1;
  strncpy(state.profile.name, name, sizeof(state.profile.name) - 1);
  state.profile.name[sizeof(state.profile.name) - 1] = '\0';
  Log.print("-> plant: ");
  Log.println(name[0] ? name : "(no profile)");
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
  Log.println("DEMO commands: 0-6 mood, d tour on/off, +/- soil, ]/[ tank, n plant, x error, f frame/memory, i wifi, h help");
}

static void handleCommand(char ch) {
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
    case 'n': plantPos = (plantPos + 1) % PLANT_COUNT; setPlant(plantPos); break;
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
  state.moistureIndex = 42;
  state.tankPercent = 68;
  setPlant(plantPos);
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
