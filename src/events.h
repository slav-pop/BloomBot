#pragma once
#include <Arduino.h>

// ============================================================
//  events.h - queue of events for the server ("tank is empty", "error", ...)
//
//  Why a queue:
//    Something happens in one module (e.g. pump.cpp sees "still dry"),
//    but sending to the server is done by net.cpp, later, and only when
//    Wi-Fi is there and the pump is off. So the module only puts the event
//    into this queue (eventPush), and net.cpp takes it out when it can send.
//    If Wi-Fi is down for a while, up to EVENT_QUEUE_SIZE events wait here;
//    when the queue is full, the OLDEST event is dropped (the newest matter more).
//
//  The event types and their extra fields are listed in Integration.md,
//  section 4. Every event has a type plus up to two numbers and one text:
//    type             value1        value2        text
//    boot             -             -             firmware version
//    watered          ml            moisture_index
//    tank_low         tank_pct
//    tank_empty       -
//    tank_refilled    tank_pct
//    error            -             -             reason (still_dry, ...)
//    error_cleared    -             -             reason that is gone
//    profile_ok       plant_id      profile_ver
//    profile_rejected -             -             reason
//    cmd_rejected     -             -             reason (cmd in value text later)
// ============================================================

struct Event {
  const char* type;     // e.g. "tank_low" - always a fixed text in the program
  int32_t value1;       // meaning depends on the type, see table above
  int32_t value2;
  const char* text;     // fixed text or nullptr
};

// Put an event into the queue (and print it to the log).
void eventPush(const char* type, int32_t value1 = 0, int32_t value2 = 0, const char* text = nullptr);

// For net.cpp: look at the oldest event without removing it.
// false = queue is empty. After a successful send: eventPop().
bool eventPeek(Event& e);
void eventPop();

int eventCount();       // how many events are waiting
void eventPrintAll();   // serial 'e': show the queue
