#include <Arduino.h>
#include "config.h"
#include "pump.h"
#include "state.h"

// TODO pump_state in state.PUMP_STATE
static bool pump_state = false;

void water_pump_on() {
    digitalWrite(PUMP_PIN, RELAIS_ACITVE_LOW ? LOW : HIGH);
    // pump_state = true;
}

void water_pump_off() {
    digitalWrite(PUMP_PIN, RELAIS_ACITVE_LOW ? HIGH : LOW);
    // pump_state = false;
}

int milliLiter_to_milliSeconds(int miliLiter) {
    // TODO: Nötig ? Praktisch testen mit länge von Schlauch etc.
    return -1;  // Kommt auf Schlauchlänge und
}

void pumpBegin() {
    
    pinMode(PUMP_PIN, OUTPUT);
    water_pump_off();
}

void pumpUpdate() {
    // if (state.needWater) {
    //     // if (state.plantGetsWater)  // first test if water reaches plant
    //     state.pumpOn = true;
    // }
    // else {
    //     state.pumpOn = false;
    // }
}