#pragma once
// ============================================================
//  wifi_ota.h — Wi-Fi connection + firmware update over Wi-Fi (ArduinoOTA).
//  The ONLY module that calls WiFi.begin(). Other modules (Telegram, server)
//  just check state.wifiConnected.
//  Writes 'state': wifiConnected, wifiConnecting.
// ============================================================
void wifiOtaBegin();
void wifiOtaUpdate();   // call every loop(), never blocks (except during an OTA upload)
void wifiOtaPrintInfo(); // serial 'i'
