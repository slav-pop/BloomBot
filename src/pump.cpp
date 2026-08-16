#include <Arduino.h>
#include "config.h"
#include "pump.h"
#include "state.h"

// TODO pump_state in state.PUMP_STATE
static bool pump_state = false;

void pumpBegin() {
    
    pinMode(PUMP_PIN, OUTPUT);
};


void pumpUpdate() {
    if (state.needWater) {
        // if (state.plantGetsWater)  // first test if water reaches plant
        state.pumpOn = true;
    }
    else {
        state.pumpOn = false;
    }
};


int milliLiter_to_milliSeconds(int miliLiter) {
    // TODO: Nötig ? Praktisch testen mit länge von Schlauch etc.
    return -1;  // Kommt auf Schlauchlänge und
}

/// TODO: Check ob richtig gesetzt wird weil Relais active Low ist -> wird woanderst ausgeführt !
void water_pump_on() {
    digitalWrite(PUMP_PIN, RELAIS_ACITVE_LOW ? LOW : HIGH);
    // pump_state = true;
}

/// TODO: Check ob richtig gesetzt wird weil Relais active Low ist -> wird woanderst ausgeführt !

void water_pump_off() {
    digitalWrite(PUMP_PIN, RELAIS_ACITVE_LOW ? HIGH : LOW);
    // pump_state = false;
}



/// PICO Test Part 1: Watertube in plant -> small water dose, changes mositure level -> else ERROR Tube might be on the ground
bool water_pump_water_plant_TEST(float moisturePercentBevor) {
    // long smallWaterAmountMS = 1000;
    // float percentChange = 0.10;  // TODO: Unterschied in Prozent nach kurzem Bewässern in der Praxsis testen
    // // water_pump_water_plant(smallWaterAmountMS);
    // // Kurzes warten schon in water_pump_water_plant enthalten
    
    // // float moisturePercentAfter = setRaw_getPercentage();
    // if (moisturePercentAfter > moisturePercentBevor + percentChange ) {
    //     return true;
    // } else {
    //     return false;
    // }
}

/// PICO Test Part 2: Watertube in plant -> small water dose, changes mositure level -> else ERROR Tube might be on the ground
void water_pump_try_water_plant(float moisturePercent, long pumpTimeMS) {
    // if (water_pump_water_plant_TEST(moisturePercent) ) {
    //     // state.plantGetsWater = true
    //     // water_pump_water_plant(pumpTimeMS);
    // }
    // else {
    //     // Go TO ERROR State
    //     // state.plantGetsWater = false
    //     // Serial.print("Wasser nicht angekommen !");
    // }
}