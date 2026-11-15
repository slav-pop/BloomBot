#pragma once
#include <Arduino.h>

// ============================================================
//  logger.h - log output to the USB cable AND over Wi-Fi (telnet)
//
//  Why:
//    Serial.print only goes into the USB cable. With the device closed in
//    its case (and at the fair) there is no cable. So every module prints
//    with Log.print / Log.println instead: the text goes to the cable as
//    before and, if someone is connected over Wi-Fi, also to them.
//    Commands (s, t, m, w, ...) can be typed there as well.
//
//  How to connect (laptop in the same Wi-Fi / hotspot as the ESP32):
//    pio device monitor --port socket://bloombot.local:23
//    (or the IP address shown by command 'i' instead of bloombot.local)
//    One client at a time: a new connection replaces the old one.
//    There is no password - only use it in our own hotspot.
//
//  Log works exactly like Serial: Log.print(...), Log.println(...),
//  also numbers with digits, e.g. Log.print(3.14159, 2).
// ============================================================

// "class LogOutput : public Print" = LogOutput is a kind of Print.
// Print is the Arduino base class that already turns numbers and texts into
// characters (Serial is a Print as well). We only have to say where the
// finished characters go: that is the function write().
class LogOutput : public Print {
public:
  size_t write(uint8_t c) override;                       // one character
  size_t write(const uint8_t* buf, size_t size) override; // many characters at once
};

extern LogOutput Log;   // the one object every module prints to

// Call every loop(): starts the telnet server once Wi-Fi is connected,
// accepts a new client, sends text that was held back while the pump ran.
void loggerUpdate();

// Next character typed by the telnet client, or -1 if there is none.
int loggerReadChar();
