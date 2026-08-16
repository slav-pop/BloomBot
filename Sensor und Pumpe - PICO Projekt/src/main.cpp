#include <Arduino.h>
#include "moisture_sensor.hpp"
#include "water_pump.hpp"

// PIN BELEGUNG (Pico GPIOs)
constexpr uint8_t SENSOR_PIN = 26; // GP26 (ADC0) -> HD38 Sensor AO Pin
constexpr uint8_t PUMP_PIN   = 15; // GP15        -> Relais IN Pin

// Schwellenwerte für die automatisierte Bewässerung
constexpr float THRESHOLD_DRY = 30.0f; // Bei unter 30% Feuchtigkeit pumpen
constexpr unsigned long PUMP_TIME_MS = 3000; // 3 Sekunden gießen

void setup() {

    // TODO Zustand "NORMAL": Check alle 1 Stunde ob Pflanze genug Wasser

    // TODO Praktisch : Aktuell PICO PINS -> Auf kompatible und freie PINS des ESP32 übertragen
    // TODO Praktisch: Funktion Gießen-Test sinnvoll und funktioniert (water_pump_water_plant_TEST)
    
    // TODO Idee Optional : Ausgabe Serial.print für jeden Zustand

    Serial.begin(115200);
    delay(2000); // Warten bis Serial stabil steht

    Serial.println("=== Automatisches Bewässerungssystem gestartet ===");

    // Initialisierung
    moisture_sensor_init(SENSOR_PIN);
    water_pump_init(PUMP_PIN, false); // true = Active-Low Relais
}

void loop() {
    int rawValue = moisture_sensor_read_raw();
    float moisturePercent = moisture_sensor_read_percentage();

    // print current state -> each state should have this !?
    // Current: State and infos to Serial Monitor
    Serial.print("Sensor Raw: ");
    Serial.print(rawValue);
    Serial.print(" | Feuchtigkeit: ");
    Serial.print(moisturePercent, 1);
    Serial.println("%");

    if (moisturePercent < THRESHOLD_DRY) { 
      // TODO: check in other State, then pump_water if in "Bewässerungsstate"
        water_pump_water_plant(PUMP_TIME_MS);
        // water_pump_try_water_plant(moisturePercent, PUMP_TIME_MS);  // Bei Endprodukt benutzen, damit Schlauch nicht am Boden liegt und Wasserpumpe unendlich weiterläuft
    }
  

    delay(2000);
    // Alle 2 Sekunden messen
    // -> Zustandscode beachten 
    // -> im normalen Modus wird alle x Minuten Stunden gemessen
    // dann aber lieber % Zeit und nicht delay !!!
}