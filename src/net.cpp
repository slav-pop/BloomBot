#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>   // HTTPS (TLS) connection
#include <HTTPClient.h>         // HTTP requests on top of it
#include <ArduinoJson.h>        // build and read JSON
#include <time.h>               // configTime(): clock from the internet
#include "config.h"
#include "secrets.h"
#include "state.h"
#include "logger.h"
#include "events.h"
#include "profile.h"
#include "pump.h"
#include "certs.h"
#include "net.h"

// ============================================================
//  net.cpp - see net.h for the overview
//
//  Blocking: an HTTPS request stops loop() - the LED ring freezes for that
//  moment (decided on purpose, see Integration.md section 2). To keep each
//  freeze short, netUpdate() sends at most ONE request per loop() run.
//
//  Keep-alive (why requests are fast most of the time):
//    Opening a new HTTPS connection takes ~4 s on the ESP32: the TLS
//    handshake checks the certificate chain (ECDSA P-384, computed in
//    software). So the connection is NOT closed after a request: 'tls' and
//    'http' below are kept, and the next request uses the same, already
//    checked connection (a few hundred ms).
//    The server closes an idle connection after a while. Then the next
//    request opens a new one (4 s again). If the kept connection turns out
//    to be dead only while sending, the request is repeated once at once
//    with a new connection.
// ============================================================

// Stop the build if the server settings are missing in secrets.h
static_assert(sizeof(SERVER_URL) > 1, "SERVER_URL is empty in src/secrets.h");
static_assert(sizeof(DEVICE_ID) > 1,  "DEVICE_ID is empty in src/secrets.h");
static_assert(sizeof(DEVICE_KEY) > 1, "DEVICE_KEY is empty in src/secrets.h");

static unsigned long pollIntervalMs = NET_POLL_DEFAULT_S * 1000UL;  // the server can change it
static unsigned long lastPollAt = 0;      // millis() of the last poll attempt
static bool pollDue = true;               // true: poll at the next chance (also right after boot)
static bool eventsWaitForPoll = false;    // sending an event failed: try again with the next poll
static bool wasConnected = false;         // Wi-Fi state in the previous loop
static bool clockStarted = false;         // configTime() was called
static int lastPollCode = 0;              // HTTP result of the last poll (for 'i')

// Kept between requests (keep-alive), see the top of this file
static WiFiClientSecure tls;              // the encrypted connection to the server
static HTTPClient http;                   // HTTP on top of it
static bool caLoaded = false;             // root certificates given to 'tls' (once)

// ---- one HTTPS POST over the kept connection (or a new one if there is none) ----
static int postOnce(const char* path, const char* body, size_t bodyLen, String& response) {
  if (!caLoaded) {
    tls.setCACert(ROOT_CA_CERTS);         // accept only a server signed by these roots (certs.h)
    caLoaded = true;
  }
  String url = String(SERVER_URL) + "/api/device/" + DEVICE_ID + path;
  if (!http.begin(tls, url)) {            // uses the open connection if there is one
    Log.println("NET: bad URL, check SERVER_URL in secrets.h");
    return -100;
  }
  http.setReuse(true);                    // keep-alive: do not close after the answer
  http.setConnectTimeout(NET_CONNECT_TIMEOUT_MS);
  http.setTimeout(NET_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " DEVICE_KEY);   // the device key, see Integration.md section 2

  int code = http.POST((uint8_t*)body, bodyLen);
  if (code > 0) response = http.getString();
  http.end();                             // with setReuse(true) the connection stays open
  return code;
}

// ---- one HTTPS POST with a JSON body ----
// Returns the HTTP status code (200 = OK) or a negative number for a
// connection error. The answer of the server is written into 'response'.
static int postJson(const char* path, const char* body, size_t bodyLen, String& response) {
  unsigned long startedAt = millis();
  bool reused = tls.connected();          // is there still an open connection from before?
  int code = postOnce(path, body, bodyLen, response);

  if (code < 0 && reused) {
    // The kept connection was already closed by the server - we only
    // noticed it while sending. Repeat once with a new connection.
    Log.println("NET: kept connection was closed by the server, reconnecting");
    tls.stop();
    reused = false;
    code = postOnce(path, body, bodyLen, response);
  }
  if (code < 0) tls.stop();               // after an error: start clean next time

  Log.print("NET: ");
  Log.print(path);
  Log.print(" -> ");
  Log.print(code);
  if (code < 0) {                         // no HTTP answer at all: say why
    Log.print(" (");
    Log.print(HTTPClient::errorToString(code).c_str());
    Log.print(")");
  }
  Log.print(", ");
  Log.print(millis() - startedAt);
  Log.print(" ms");
  Log.println(reused ? " (kept connection)" : " (new connection)");
  return code;
}

// ---- the status JSON (Integration.md section 3) ----
static size_t buildStatus(char* buf, size_t size) {
  JsonDocument doc;
  doc["fw"] = FW_VERSION;
  doc["uptime_s"] = millis() / 1000UL;
  doc["moisture_index"] = state.moistureIndex;
  JsonArray sensors = doc["moisture_sensors"].to<JsonArray>();   // both sensors one by one
  sensors.add(state.moistureSensorIndex[0]);
  sensors.add(state.moistureSensorIndex[1]);
  JsonArray raw = doc["moisture_raw"].to<JsonArray>();           // raw ADC, for calibration
  raw.add(state.moistureRaw[0]);
  raw.add(state.moistureRaw[1]);
  doc["tank_pct"] = state.tankPercent;
  doc["tank_low"] = state.tankLow;
  doc["tank_capacity_ml"] = TANK_CAPACITY_ML;
  doc["total_pumped_ml"] = state.totalPumpedMl;
  doc["phase"] = waterPhaseName(state.waterPhase);
  doc["pump_on"] = state.pumpOn;
  doc["need_water"] = state.needWater;
  doc["error"] = state.errorFlag || state.tankSensorFail || state.moistureSensorFail;
  if (state.errorReason)              doc["error_reason"] = state.errorReason;
  else if (state.tankSensorFail)      doc["error_reason"] = "sensor_tank";
  else if (state.moistureSensorFail)  doc["error_reason"] = "sensor_moisture";
  else                                doc["error_reason"] = nullptr;   // JSON null
  doc["attempts"] = state.waterAttempts;
  doc["plant_id"] = state.profile.plantId;
  doc["profile_ver"] = state.profile.ver;
  doc["last_dose_ml"] = state.lastDoseMl;
  doc["last_watered_s_ago"] = state.wateredSinceBoot ? (long)((millis() - state.lastWateredAt) / 1000UL) : -1L;
  doc["rssi"] = WiFi.RSSI();
  return serializeJson(doc, buf, size);
}

// ---- one event as JSON (Integration.md section 4) ----
static size_t buildEvent(const Event& e, char* buf, size_t size) {
  JsonDocument doc;
  doc["type"] = e.type;
  const char* t = e.type;
  if      (!strcmp(t, "boot"))             { doc["fw"] = e.text; }
  else if (!strcmp(t, "watered"))          { doc["ml"] = e.value1; doc["moisture_index"] = e.value2; }
  else if (!strcmp(t, "tank_low") ||
           !strcmp(t, "tank_refilled"))    { doc["tank_pct"] = e.value1; }
  else if (!strcmp(t, "error") ||
           !strcmp(t, "error_cleared") ||
           !strcmp(t, "profile_rejected")) { doc["reason"] = e.text; }
  else if (!strcmp(t, "profile_ok"))       { doc["plant_id"] = e.value1; doc["profile_ver"] = e.value2; }
  else if (!strcmp(t, "cmd_rejected"))     { doc["cmd"] = "reset"; doc["reason"] = e.text; }  // reset is the only command
  // tank_empty: no extra fields
  return serializeJson(doc, buf, size);
}

// ---- profile from the poll answer -> profileApply() ----
// The numbers are first read as plain int and checked for the size of the
// fields in struct Profile (e.g. moistMin is a uint8_t = 0..255). Without
// this check a value like 300 would silently wrap around to 44.
static void handleProfile(JsonObject p) {
  const char* reason = nullptr;
  bool ok = p["profile_ver"].is<int>() && p["plant_id"].is<int>() &&
            p["moist_min"].is<int>() && p["moist_target"].is<int>() && p["moist_max"].is<int>() &&
            p["dose_ml"].is<int>() && p["min_pause_h"].is<int>();
  if (!ok) {
    reason = "missing or non-numeric field";
  } else {
    int ver = p["profile_ver"], mn = p["moist_min"], tg = p["moist_target"], mx = p["moist_max"];
    int dose = p["dose_ml"], pause = p["min_pause_h"];
    if (ver < 1 || ver > 65535 || mn < 0 || mn > 255 || tg < 0 || tg > 255 || mx < 0 || mx > 255 ||
        dose < 0 || dose > 65535 || pause < 0 || pause > 65535) {
      reason = "value out of range";
    } else {
      Profile np;
      profileDefaults(np);
      np.ver = ver;
      np.plantId = p["plant_id"];
      strncpy(np.name, p["name"] | "", sizeof(np.name) - 1);    // "| x" = use x if the field is missing
      strncpy(np.mode, p["mode"] | "normal", sizeof(np.mode) - 1);
      np.moistMin = mn;
      np.moistTarget = tg;
      np.moistMax = mx;
      np.doseMl = dose;
      np.minPauseH = pause;
      if (profileApply(np, &reason)) {
        eventPush("profile_ok", np.plantId, np.ver);
        return;
      }
    }
  }
  Log.print("NET: profile rejected - ");
  Log.println(reason);
  eventPush("profile_rejected", 0, 0, reason);
}

// ---- commands from the poll answer (Integration.md section 6) ----
static void handleCommands(JsonArray cmds) {
  for (JsonObject c : cmds) {
    const char* name = c["cmd"] | "";
    if (!strcmp(name, "reset")) {
      if (state.waterPhase == PHASE_ERROR) {
        Log.println("NET: command reset");
        state.errorResetRequest = true;    // pump.cpp handles it in the next loop
      } else {
        eventPush("cmd_rejected", 0, 0, "not in ERROR");
      }
    } else if (!strcmp(name, "factory_reset")) {
      // device was reset in the bot: forget the plant profile
      Log.println("NET: command factory_reset");
      profileFactoryReset();
    } else {
      Log.print("NET: unknown command ");
      Log.println(name);
    }
  }
}

// ---- send the status, read the answer ----
static void doPoll() {
  lastPollAt = millis();
  pollDue = false;
  eventsWaitForPoll = false;               // after the poll, waiting events may try again

  char body[NET_JSON_SIZE];
  size_t len = buildStatus(body, sizeof(body));
  String response;
  lastPollCode = postJson("/poll", body, len, response);
  if (lastPollCode != 200) return;         // try again after the normal interval

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response.c_str());
  if (err) {
    Log.print("NET: bad JSON from server: ");
    Log.println(err.c_str());
    return;
  }

  // poll_s: accept only 60..3600 s, otherwise keep the default (Integration.md section 2)
  int pollS = doc["poll_s"] | NET_POLL_DEFAULT_S;
  if (pollS < NET_POLL_MIN_S || pollS > NET_POLL_MAX_S) pollS = NET_POLL_DEFAULT_S;
  pollIntervalMs = (unsigned long)pollS * 1000UL;

  if (doc["profile"].is<JsonObject>()) handleProfile(doc["profile"].as<JsonObject>());
  if (doc["commands"].is<JsonArray>()) handleCommands(doc["commands"].as<JsonArray>());
}

// ---- send the oldest waiting event ----
static void sendOneEvent() {
  Event e;
  if (!eventPeek(e)) return;
  char body[NET_JSON_SIZE];
  size_t len = buildEvent(e, body, sizeof(body));
  String response;
  int code = postJson("/event", body, len, response);
  if (code == 200) {
    eventPop();                            // delivered: remove from the queue
  } else {
    eventsWaitForPoll = true;              // keep it, try again with the next poll
  }
}

void netUpdate() {
  bool connected = state.wifiConnected;
  if (connected && !wasConnected) {
    pollDue = true;                        // just (re)connected: report at once
    if (!clockStarted) {
      // Real time from the internet (runs in the background). HTTPS checks
      // the validity dates of certificates, which needs a clock.
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      clockStarted = true;
    }
  }
  if (!connected && wasConnected) tls.stop();   // Wi-Fi lost: the kept connection is dead
  wasConnected = connected;
  if (!connected) return;

  // Pump safety: no network request while the pump runs (Integration.md section 2)
  if (state.pumpOn) return;

  // One request per loop() at most - keeps every LED freeze short
  if (pollDue || millis() - lastPollAt >= pollIntervalMs) {
    doPoll();
  } else if (eventCount() > 0 && !eventsWaitForPoll) {
    sendOneEvent();
  }
}

void netPollNow() {
  pollDue = true;
}

void netPrintInfo() {
  Log.print("NET: server ");
  Log.print(SERVER_URL);
  Log.print(", device ");
  Log.print(DEVICE_ID);
  Log.print(", poll every ");
  Log.print(pollIntervalMs / 1000UL);
  Log.print(" s, last poll result ");
  Log.print(lastPollCode);
  Log.print(", events waiting ");
  Log.println(eventCount());
}
