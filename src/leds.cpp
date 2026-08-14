// leds.cpp - all effects and rendering logic
// The module reads state.ledStatus and renders the appropriate animation.

#include <FastLED.h>
#include "config.h"
#include "state.h"
#include "leds.h"

static CRGB leds[TOTAL_LEDS];
static uint8_t hue = 0;

static LedStatus previousStatus = STATUS_IDLE;
static unsigned long statusChangedAt = 0;   // the moment of transition to the current status

// ===== Needs watering: slow red "breathing" =====
static void effectNeedWater() {
  uint8_t breath = beatsin8(10, 15, 255);  // 10 BPM - slow breath in/out
  fill_solid(leds, ACTIVE_LEDS, CRGB(breath, 0, 0));
}

// ===== Watering is in progress =====
static void effectWatering() {
  static uint8_t pos = 0;
  static unsigned long lastStep = 0;
  const unsigned long stepInterval = 90;   // ms between steps (speed)

  fadeToBlackBy(leds, ACTIVE_LEDS, 25);    // fading tail
  leds[pos] = CRGB(0, 140, 255);           // the bright head of the comet

  if (millis() - lastStep >= stepInterval) {
    lastStep = millis();
    pos = (pos + 1) % ACTIVE_LEDS;
  }
}

// ===== Low water level in the tank =====
static void effectLowWater() {
  uint8_t breath = beatsin8(20, 10, 255);
  CRGB c = CRGB(255, 140, 0);
  c.nscale8_video(breath);
  fill_solid(leds, ACTIVE_LEDS, c);
}

// ===== Connecting to Wi-Fi/Telegram =====
static void effectWifiConnecting() {
  static int pos = 0;                 // current position of the bar
  static int dir = 1;                 // direction: 1 to the right, -1 to the left
  static unsigned long lastStep = 0;
 
  const unsigned long edgeInterval   = 90;  // ms/step AT THE EDGES (slowly)
  const unsigned long centerInterval = 12;  // ms/step IN THE MIDDLE (fast)

 
  const int center = (ACTIVE_LEDS - 1) / 2;
  int distFromCenter = abs(pos - center);  // 0 in the center, maximum at the edges
  unsigned long stepInterval =
      map(distFromCenter, 0, center, centerInterval, edgeInterval);
 
  fadeToBlackBy(leds, ACTIVE_LEDS, 90);  // a faint trail behind the strip
 
  // A strip 3 diodes wide around pos
  for (int i = pos - 1; i <= pos + 1; i++) {
    if (i >= 0 && i < ACTIVE_LEDS) leds[i] = CRGB::White;
  }
 
  if (millis() - lastStep >= stepInterval) {
    lastStep = millis();
    pos += dir;
    if (pos <= 0)                    { pos = 0;               dir = 1;  }
    else if (pos >= ACTIVE_LEDS - 1) { pos = ACTIVE_LEDS - 1; dir = -1; }
  }
}

// ===== All is OK =====
static void effectOkay() {
  uint8_t breath = beatsin8(8, 10, 255);
  CRGB c = CRGB(37, 173, 87);
  c.nscale8_video(breath);
  fill_solid(leds, ACTIVE_LEDS, c);
}


// ===== Critical Error =====
static void effectError() {
  const unsigned long period = 1400;       // total cycle time, ms
  unsigned long t = millis() % period;

  uint8_t b = 0;                           // brightness: 0 = darkness (pause)
  if (t < 200) {
    b = 255 - (t * 255 / 200);             // 1st pulse: flash and decay over 200 ms
  } else if (t >= 300 && t < 500) {
    b = 255 - ((t - 300) * 255 / 200);     // 2nd pulse: 100 ms after the first
  }
  // from 500 to 1400 ms - silence between pulses

  fill_solid(leds, ACTIVE_LEDS, CRGB(b, 0, 0));
}



// ===== Rainbow/IDLE =====
static void effectIdle() {
  fill_rainbow(leds, ACTIVE_LEDS, hue, 255 / ACTIVE_LEDS);  // full circle of colors
  EVERY_N_MILLISECONDS(30) { hue += 2; }
}



// ======================================================
void ledsBegin() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, TOTAL_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.clear(true);
}

void ledsUpdate() {
  // recording the moment when the status changes (needed for effects with acceleration)
  if (state.ledStatus != previousStatus) {
    statusChangedAt = millis();
    previousStatus = state.ledStatus;
  }

  switch (state.ledStatus) {
    case STATUS_NEED_WATER:
      effectNeedWater();
      break;
    case STATUS_WATERING:
      effectWatering();
      break;
    case STATUS_LOW_WATER:
      effectLowWater();
      break;
    case STATUS_WIFI_CONNECTING:
      effectWifiConnecting();
      break;
    case STATUS_OKAY:
      effectOkay();
      break;
    case STATUS_ERROR:
      effectError();
      break;
    default:
      effectIdle();
      break;
  }

  // unused strip tail is always turned off
  fill_solid(leds + ACTIVE_LEDS, TOTAL_LEDS - ACTIVE_LEDS, CRGB::Black);
  FastLED.show();
}
