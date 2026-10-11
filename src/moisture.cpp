#include <Arduino.h>
#include "config.h"
#include "moisture.h"
#include "state.h"
#include <algorithm>  // For std::sort

// Test Werte mit Wasser
// SHOULD BE correct in config.h -> Cannot test no ESP32
// static const int DRY_VAL = 1023;   // 1023
// static const int WET_VAL = 372;  // 372 - 486 ( 529 ) -> Alles komplett im Wasser

static unsigned long lastReadAt = 0;

int moisture_sensor_read_raw() {
    return analogRead(MOISTURE_PIN);
}

int moisture_sensor_read_raw_median(uint8_t samples = 5, uint16_t delay_ms = 1) {
    if (samples == 0) return 0;  // Edge case: No samples

    // Allocate array for readings
    int readings[samples];

    // Take samples
    for (int i = 0; i < samples; i++) {
        readings[i] = moisture_sensor_read_raw();
        if (i < samples - 1) delay(delay_ms);  // No delay after last reading
    }

    // Sort the array
    std::sort(readings, readings + samples);

    // Return median
    if (samples % 2 == 1) {
        // Odd number of samples: Return middle value
        return readings[samples / 2];
    } else {
        // Even number of samples: Return average of two middle values
        return (readings[samples / 2 - 1] + readings[samples / 2]) / 2;
    }
}

/// percentage depends on moistureRaw -> should always set together, else percent is depending on other moistureRaw value
float setRaw_getPercentage() {
    // int raw = moisture_sensor_read_raw_median();  // PICO CODE
    state.moistureRaw = moisture_sensor_read_raw_median();
    
    // Map-Funktion: Trocken = 0%, Nass = 100%
    float percentage = map(state.moistureRaw, DRY_VAL, WET_VAL, 0, 100);

    // Begrenzen auf 0 - 100%
    return constrain(percentage, 0.0f, 100.0f);
}

void moistureUpdate() {
    // Non-blocking timer: exit if the measurement time hasn't come yet
    if (millis() - lastReadAt < MOISTURE_READ_INTERVAL_MS) return;
    lastReadAt = millis();

    float moisturePercent = setRaw_getPercentage();

    if (moisturePercent < THRESHOLD_DRY) {
        state.needWater = true;
    }
    else {
        state.needWater = false;
    }
}

void moistureBegin() {
    pinMode(MOISTURE_POWER_PIN, OUTPUT);
    digitalWrite(MOISTURE_POWER_PIN, HIGH);   // power the sensor (always on for now)
    pinMode(MOISTURE_PIN, INPUT);
    analogReadResolution(12);   // ESP32: 0..4095
}