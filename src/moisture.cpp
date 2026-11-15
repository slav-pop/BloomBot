#include <Arduino.h>
#include "config.h"
#include "moisture.h"
#include "state.h"
#include <algorithm>  // std::sort

// ============================================================
//  moisture.cpp - two resistive moisture sensors (HD-38) in one pot.
//
//  Non-blocking measurement:
//    moistureRequest()  -> power on, remember the time
//    moistureUpdate()   -> after MOISTURE_POWER_ON_MS: read both sensors
//                          (~10 ms), power off, state.moistureSeq++
//  Who asked for a measurement waits until state.moistureSeq changes.
//
//  Combining:
//    both OK     -> moistureIndex = average
//    one failed  -> moistureIndex = the other one
//    both failed -> moistureIndex = -1, moistureSensorFail = true (no watering)
//    difference > MOISTURE_MISMATCH_DIFF -> moistureMismatch = true (watering by average)
// ============================================================

static const uint8_t SENSOR_PINS[MOISTURE_SENSORS] = { MOISTURE_PIN_1, MOISTURE_PIN_2 };
static const int DRY[MOISTURE_SENSORS] = { DRY_VAL_1, DRY_VAL_2 };
static const int WET[MOISTURE_SENSORS] = { WET_VAL_1, WET_VAL_2 };

static bool measuring = false;          // power is on, waiting for the sensors to settle
static unsigned long powerOnAt = 0;

// ---- median of several samples; -1 = implausible reading (sensor failed) ----
static int readSensorRaw(uint8_t pin) {
    int r[MOISTURE_SAMPLES];
    for (int i = 0; i < MOISTURE_SAMPLES; i++) {
        r[i] = analogRead(pin);
        if (i < MOISTURE_SAMPLES - 1) delay(1);
    }
    std::sort(r, r + MOISTURE_SAMPLES);

    // A probe in soil gives steady values. Values jumping around = loose wire
    // or a floating ADC pin.
    if (r[MOISTURE_SAMPLES - 1] - r[0] > MOISTURE_MAX_SPREAD) return -1;

    return r[MOISTURE_SAMPLES / 2];
}

// ---- raw ADC -> index 0..100 (0 = dry soil, 100 = freshly watered), own calibration per sensor ----
static int rawToIndex(int raw, int i) {
    long p = map(raw, DRY[i], WET[i], 0, 100);
    return (int)constrain(p, 0L, 100L);
}

void moistureRequest() {
    if (measuring) return;                  // already running: the caller gets that result
    // Power only while measuring: a resistive probe under constant voltage
    // corrodes within weeks.
    digitalWrite(MOISTURE_POWER_PIN, HIGH);
    powerOnAt = millis();
    measuring = true;
}

bool moistureBusy() {
    return measuring;
}

void moistureUpdate() {
    if (!measuring) return;
    // The module needs time after power on (capacitor on the board):
    // with 30 ms a dry sensor read ~2700 instead of 4095
    if (millis() - powerOnAt < MOISTURE_POWER_ON_MS) return;

    for (int i = 0; i < MOISTURE_SENSORS; i++) {
        int raw = readSensorRaw(SENSOR_PINS[i]);
        state.moistureRaw[i] = raw;
        state.moistureSensorIndex[i] = raw < 0 ? -1 : rawToIndex(raw, i);
    }
    digitalWrite(MOISTURE_POWER_PIN, LOW);
    measuring = false;

    int a = state.moistureSensorIndex[0];
    int b = state.moistureSensorIndex[1];
    if (a >= 0 && b >= 0) {
        state.moistureIndex = (a + b + 1) / 2;
        state.moistureMismatch = abs(a - b) > MOISTURE_MISMATCH_DIFF;
    } else {
        state.moistureIndex = (a >= 0) ? a : b;   // the working one, or -1
        state.moistureMismatch = false;
    }
    state.moistureSensorFail = (state.moistureIndex < 0);
    state.moistureSeq++;                    // signal: a new result is ready
}

void moistureBegin() {
    pinMode(MOISTURE_POWER_PIN, OUTPUT);
    digitalWrite(MOISTURE_POWER_PIN, LOW);    // off until the first measurement
    for (int i = 0; i < MOISTURE_SENSORS; i++) pinMode(SENSOR_PINS[i], INPUT);
    analogReadResolution(12);   // ESP32: 0..4095
}