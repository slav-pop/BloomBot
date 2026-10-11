#include <Arduino.h>
#include "config.h"
#include "pump.h"
#include "state.h"

static unsigned long pumpStartedAt = 0;   // when the pump was switched on

// ---- the only place where the relay pin is written ----
static void water_pump_on() {
    digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
    state.pumpOn = true;
    pumpStartedAt = millis();
}

static void water_pump_off() {
    digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
    state.pumpOn = false;
}

int milliLiter_to_milliSeconds(int miliLiter) {
    // TODO: test in practice with the real tube length
    return -1;
}

void pumpBegin() {
    pinMode(PUMP_PIN, OUTPUT);
    water_pump_off();
}

void pumpUpdate() {
    // Test run on request (serial 'p'). Ignored if the pump is already running.
    if (state.pumpTestRequest && !state.pumpOn) {
        water_pump_on();
        Serial.println("PUMP: ON (test)");
    }
    state.pumpTestRequest = false;   // request is one-shot

    // Switch off after PUMP_TEST_MS
    if (state.pumpOn && millis() - pumpStartedAt >= PUMP_TEST_MS) {
        water_pump_off();
        Serial.println("PUMP: OFF");
    }
    // if (state.needWater) {
    //     // if (state.plantGetsWater)  // first test if water reaches plant
    //     state.pumpOn = true;
    // }
    // else {
    //     state.pumpOn = false;
    // }
}