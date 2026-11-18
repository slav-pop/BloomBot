#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "state.h"
#include "logger.h"

// ============================================================
//  logger.cpp - see logger.h for what it is for
//
//  Pump safety:
//    Sending over Wi-Fi can block for a moment if the network is slow.
//    The pump must switch off on time, so while state.pumpOn is true the
//    text for the telnet client is only collected in a buffer (pending[])
//    and sent when the pump is off again. The cable gets everything at once.
//    If more than LOG_BUFFER_SIZE characters come while pumping, the rest
//    is dropped for telnet and a short note is sent instead.
// ============================================================

LogOutput Log;

static WiFiServer server(TELNET_PORT);   // waits for incoming connections
static WiFiClient client;                // the one connected client (if any)
static bool serverRunning = false;

static char pending[LOG_BUFFER_SIZE];    // text held back while the pump runs
static size_t pendingLen = 0;
static bool pendingLost = false;         // something did not fit into pending[]

static uint8_t skipBytes = 0;            // see loggerReadChar()

static bool clientConnected() {
  return client && client.connected();
}

// ---- send what was collected while the pump was running ----
static void sendPending() {
  if (pendingLen == 0) return;
  client.write((const uint8_t*)pending, pendingLen);
  pendingLen = 0;
  if (pendingLost) {
    client.print("[log: some lines were lost while the pump was running]\r\n");
    pendingLost = false;
  }
}

// ---- where every character printed with Log ends up ----
size_t LogOutput::write(const uint8_t* buf, size_t size) {
  Serial.write(buf, size);                 // the cable always gets it

  if (!clientConnected()) return size;     // nobody listening over Wi-Fi

  if (state.pumpOn) {                      // pump running: only collect, never block
    size_t room = sizeof(pending) - pendingLen;
    size_t n = size < room ? size : room;
    memcpy(pending + pendingLen, buf, n);
    pendingLen += n;
    if (n < size) pendingLost = true;
    return size;
  }

  sendPending();                           // older text first, keeps the order
  client.write(buf, size);
  return size;
}

size_t LogOutput::write(uint8_t c) {
  return write(&c, 1);
}

void loggerUpdate() {
  // No Wi-Fi: stop everything, it is started again after reconnecting
  if (!state.wifiConnected) {
    if (serverRunning) {
      client.stop();
      server.end();
      serverRunning = false;
      pendingLen = 0;
    }
    return;
  }

  if (!serverRunning) {
    server.begin();
    server.setNoDelay(true);               // send small pieces right away
    serverRunning = true;
    Log.print("LOG: telnet ready on port ");
    Log.println(TELNET_PORT);
  }

  // A new client knocks: it replaces the old one
  if (server.hasClient()) {
    if (clientConnected()) client.stop();
    client = server.available();
    client.setNoDelay(true);
    pendingLen = 0;
    skipBytes = 0;
    Log.println("LOG: telnet client connected. Commands: s t m w r i n p T e F f a 0-6, demo: - + D [ ] L V H o G j");
  }

  if (!state.pumpOn && clientConnected()) sendPending();
}

int loggerReadChar() {
  while (clientConnected() && client.available()) {
    int c = client.read();
    if (c < 0) return -1;
    // Real telnet programs first send control sequences: byte 255 ("IAC")
    // followed by two more bytes. They are not commands - skip them.
    if (skipBytes > 0) { skipBytes--; continue; }
    if (c == 255)      { skipBytes = 2; continue; }
    return c;
  }
  return -1;
}
