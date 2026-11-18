#pragma once
// ============================================================
//  net.h - connection to the server (Cloudflare Worker)
//
//  What it does (details: Integration.md, sections 2-6):
//    - every poll_s seconds: send the status (POST /api/device/<id>/poll);
//      the answer can contain a new plant profile and commands
//    - events from the queue (events.h): send each one right away
//      (POST /api/device/<id>/event)
//    - never any network request while the pump is running
//
//  The device ALWAYS starts the connection: the server cannot reach the
//  ESP32 (it has no public address behind the router / hotspot).
//  Without Wi-Fi or server the device simply keeps watering on its own.
// ============================================================
void netUpdate();         // call every loop(); does nothing most of the time
void netPollNow();        // serial 'n': send the status at the next loop
void netPrintInfo();      // serial 'i' (after the Wi-Fi info): poll interval, last result
