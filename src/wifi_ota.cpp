#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "secrets.h"
#include "state.h"
#include "pump.h"
#include "wifi_ota.h"

// Empty Wi-Fi or OTA password = a firmware nobody can update without a cable.
// Stop the build instead of flashing such a firmware.
static_assert(sizeof(WIFI_SSID) > 1,    "WIFI_SSID is empty in src/secrets.h");
static_assert(sizeof(OTA_PASSWORD) > 1, "OTA_PASSWORD is empty in src/secrets.h");

static bool wasConnected = false;      // connection state in the previous loop
static bool otaRunning = false;        // ArduinoOTA.begin() was called
static unsigned long lastAttemptAt = 0;

static void connectWifi() {
  lastAttemptAt = millis();
  state.wifiConnecting = true;
  Serial.print("WIFI: connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// (Re)start OTA after every (re)connect: mDNS must be announced again
static void startOta() {
  if (otaRunning) ArduinoOTA.end();
  ArduinoOTA.begin();
  otaRunning = true;
  Serial.print("OTA: ready, upload to ");
  Serial.print(OTA_HOSTNAME);
  Serial.print(".local or ");
  Serial.println(WiFi.localIP());
}

void wifiOtaBegin() {
  WiFi.setHostname(OTA_HOSTNAME);   // name in the router list
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);             // more reliable OTA and mDNS
  WiFi.setAutoReconnect(false);     // we reconnect ourselves in wifiOtaUpdate()

  ArduinoOTA.setHostname(OTA_HOSTNAME);   // -> bloombot.local
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    pumpLock();                     // FIRST: pump off until reboot
    Serial.println("OTA: start (pump locked)");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int lastTen = -1;
    int ten = done * 10 / total;
    if (ten != lastTen) {
      lastTen = ten;
      Serial.print("OTA: ");
      Serial.print(ten * 10);
      Serial.println(" %");
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: done, rebooting");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA: error ");
    Serial.print(error);
    Serial.println(" - old firmware keeps running, pump locked until reboot");
  });

  connectWifi();
}

void wifiOtaUpdate() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    if (!wasConnected) {            // just (re)connected
      wasConnected = true;
      state.wifiConnected = true;
      state.wifiConnecting = false;
      Serial.print("WIFI: connected, IP ");
      Serial.println(WiFi.localIP());
      startOta();
    }
    ArduinoOTA.handle();
    return;
  }

  if (wasConnected) {               // just lost the connection
    wasConnected = false;
    state.wifiConnected = false;
    Serial.println("WIFI: connection lost");
    connectWifi();
    return;
  }

  if (millis() - lastAttemptAt >= WIFI_RETRY_MS) connectWifi();
}

void wifiOtaPrintInfo() {
  Serial.print("WIFI: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("connected to ");
    Serial.print(WIFI_SSID);
    Serial.print(", IP ");
    Serial.print(WiFi.localIP());
    Serial.print(", RSSI ");
    Serial.print(WiFi.RSSI());
    Serial.print(" dBm, OTA ");
    Serial.print(OTA_HOSTNAME);
    Serial.println(".local");
  } else {
    Serial.println("not connected");
  }
}
