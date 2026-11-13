#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "water_tank.h"

// ============================================================
//  water_tank.cpp — ultrasonic water level sensor.
//
//  Three techniques to prevent "garbage" readings:
//  1. A short pulseIn timeout (12 ms ≈ max 2 m) so that
//     an incorrect measurement doesn’t freeze the strip animation for too long.
//  2. A median filter based on the last 3 readings: a single outlier
//     (echo from the tank wall, interference) is simply discarded.
//  3. Hysteresis for the 'tankLow' flag: enabled below 20%,
//     disabled only above 25%—the status does not flicker at the threshold.
// ============================================================

static unsigned long lastReadAt = 0;         // last measurement
static unsigned long lastPumpActiveAt = 0;   // when pump was last active

// "circle" buffer of the last three valid measurements 0→1→2→0→1→2
static float samples[3] = { -1, -1, -1};
static uint8_t sampleIndex = 0;

static uint8_t failCount = 0;                // failed readings in a row

// ---- one "raw" distance measurement, cm; -1 for an error ----
static float readDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);     // PIN reset
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 12,000 µs timeout: up to ~2 m
  long duration = pulseIn(ECHO_PIN, HIGH, 12000);
  if (duration == 0) return -1;     // no data

  return duration * 0.034f / 2.0f;  // 0.034 - sound speed, 2 - distance back and forth
}

// ---- The median of three values ----
static float median3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }
  if (b > c) { float t = b; b = c; c = t; }
  if (a > b) { float t = a; a = b; b = t; }
  return b;    
}

// ---- Converting distance to a percentage (0..100) ----
static int distanceToPercent(float distCm) {
  // limit the range to the calibration range so we don't go beyond 0..100
  distCm = constrain(distCm, TANK_DIST_FULL_CM, TANK_DIST_EMPTY_CM);

  float span = TANK_DIST_EMPTY_CM - TANK_DIST_FULL_CM;
  return (int)(100.0f * (TANK_DIST_EMPTY_CM - distCm) / span + 0.5f);
}

// ============================================================

void tankBegin() {
  pinMode(TANK_POWER_PIN, OUTPUT);
  digitalWrite(TANK_POWER_PIN, HIGH);   // power the sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void tankUpdate() {
  // Remember when the pump was active
  if (state.pumpOn) lastPumpActiveAt = millis();

  // Fast if pump is on OR turned off recently; otherwise slow
  bool fastMode = state.pumpOn ||
                  (millis() - lastPumpActiveAt < TANK_FAST_WINDOW_MS);
  unsigned long interval = fastMode ? TANK_READ_FAST_MS : TANK_READ_SLOW_MS;

  // Non-blocking timer: exit if the measurement time hasn't come yet
  if (millis() - lastReadAt < interval) return;
  lastReadAt = millis();

  float d = readDistanceOnce();
  if (d < 0) {
    // One bad reading is normal (echo lost). Many in a row = sensor broken
    // or disconnected: then the old value must not stay in 'state',
    // otherwise the pump keeps trusting a level that is no longer measured.
    if (failCount < 255) failCount++;
    if (failCount >= TANK_MAX_FAILS && !state.tankSensorFail) {
      state.tankSensorFail = true;
      state.tankDistanceCm = -1;
      state.tankPercent = -1;              // pump.cpp: -1 = no water -> pump blocked
      samples[0] = samples[1] = samples[2] = -1;
      Serial.println("TANK: sensor failed (no echo), level unknown, pump blocked");
    }
    return;
  }

  failCount = 0;
  if (state.tankSensorFail) {
    state.tankSensorFail = false;
    Serial.println("TANK: sensor OK again");
  }

  samples[sampleIndex] = d;
  sampleIndex = (sampleIndex + 1) % 3;    // changing in cicle

  // while the buffer is not full (the first 2 measurements) — using the raw value
  float filtered;
  if (samples[0] < 0 || samples[1] < 0 || samples[2] < 0) {
    filtered = d;
  } else {
    filtered = median3(samples[0], samples[1], samples[2]);
  }

  // --- Post the results to the shared state ---
  state.tankDistanceCm = filtered;
  state.tankPercent = distanceToPercent(filtered);

  // Hysteresis for the "low water" flag
  if (!state.tankLow && state.tankPercent < TANK_LOW_ON_PERCENT) {
    state.tankLow = true;
  } else if (state.tankLow && state.tankPercent > TANK_LOW_OFF_PERCENT) {
    state.tankLow = false;
  }
}