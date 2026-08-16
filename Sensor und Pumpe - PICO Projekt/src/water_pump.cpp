#include "water_pump.hpp"
#include "moisture_sensor.hpp"

static uint8_t pump_pin;
static bool is_active_low = true;
static bool pump_state = false;



void water_pump_init(uint8_t pin, bool activeLow) {
    pump_pin = pin;
    is_active_low = activeLow;
    
    pinMode(pump_pin, OUTPUT);
    water_pump_off(); // Beim Start sicherheitshalber ausschalten
}

bool water_pump_water_plant_TEST(float moisturePercentBevor) {
    long smallWaterAmountMS = 1000;
    float percentChange = 0.10;  // TODO: Unterschied in Prozent nach kurzem Bewässern in der Praxsis testen
    water_pump_water_plant(smallWaterAmountMS);
    // Kurzes warten schon in water_pump_water_plant enthalten
    
    float moisturePercentAfter = moisture_sensor_read_percentage();
    if (moisturePercentAfter > moisturePercentBevor + percentChange ) {
        return true;
    } else {
        return false;
    }
}

void water_pump_try_water_plant(float moisturePercent, long pumpTimeMS) {
    if (water_pump_water_plant_TEST(moisturePercent) ) {
        water_pump_water_plant(pumpTimeMS);
    }
    else {
        // Go TO ERROR State
        Serial.print("Wasser nicht angekommen !");
    }
}

void water_pump_water_plant(long pumpTimeMS) {
    // Automatische Bewässerungslogik
    Serial.println("⚠️ Boden zu trocken! Starte Pumpe...");
    water_pump_on();
    delay(pumpTimeMS);
    water_pump_off();
    Serial.println("✅ Bewässerung beendet. Pause zum Sickern...");
    delay(5000); // 5 Sekunden warten, damit sich das Wasser im Boden verteilt
}


int milliLiter_to_milliSeconds(int miliLiter) {
    // TODO: Praktisch testen
    return -1;  // Kommt auf Schlauchlänge und
}


void water_pump_on() {
    digitalWrite(pump_pin, is_active_low ? LOW : HIGH);
    pump_state = true;
}

void water_pump_off() {
    digitalWrite(pump_pin, is_active_low ? HIGH : LOW);
    pump_state = false;
}

bool water_pump_is_on() {
    return pump_state;
}
