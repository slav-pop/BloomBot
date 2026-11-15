#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "events.h"

// ============================================================
//  events.cpp - a small ring buffer
//
//  "Ring buffer": a fixed array of EVENT_QUEUE_SIZE places. 'head' points
//  to the oldest event, 'count' says how many are stored. New events are
//  written behind the last one; at the end of the array it continues at
//  place 0 again (that is the "ring"). No memory is allocated at runtime.
// ============================================================

static Event queue[EVENT_QUEUE_SIZE];
static int head = 0;      // index of the oldest event
static int count = 0;     // number of stored events

static void printEvent(const Event& e) {
  Log.print(e.type);
  Log.print(" (");
  Log.print(e.value1);
  Log.print(", ");
  Log.print(e.value2);
  if (e.text) {
    Log.print(", ");
    Log.print(e.text);
  }
  Log.print(")");
}

void eventPush(const char* type, int32_t value1, int32_t value2, const char* text) {
  if (count == EVENT_QUEUE_SIZE) {          // full: drop the oldest
    Log.print("EVENT: queue full, dropped ");
    printEvent(queue[head]);
    Log.println();
    head = (head + 1) % EVENT_QUEUE_SIZE;
    count--;
  }
  int tail = (head + count) % EVENT_QUEUE_SIZE;   // first free place
  queue[tail] = { type, value1, value2, text };
  count++;

  Log.print("EVENT: ");
  printEvent(queue[tail]);
  Log.println();
}

bool eventPeek(Event& e) {
  if (count == 0) return false;
  e = queue[head];
  return true;
}

void eventPop() {
  if (count == 0) return;
  head = (head + 1) % EVENT_QUEUE_SIZE;
  count--;
}

int eventCount() {
  return count;
}

void eventPrintAll() {
  Log.print("EVENTS waiting: ");
  Log.println(count);
  for (int i = 0; i < count; i++) {
    Log.print("  ");
    printEvent(queue[(head + i) % EVENT_QUEUE_SIZE]);
    Log.println();
  }
}
