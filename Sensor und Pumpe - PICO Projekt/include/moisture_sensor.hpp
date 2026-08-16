#pragma once
#include <Arduino.h>

/// Initialisiert den HD38 Feuchtigkeitssensor.
/// \param pin Analoger Pin des Pico (z.B. 26 für GP26 / ADC0)
void moisture_sensor_init(uint8_t pin);

/// Liest den Rohwert des ADC aus (0 - 1023 bei Arduino-Standard bzw. 0 - 4095).
int moisture_sensor_read_raw();

/// Ließt mehrfach und berechnet den median, um Fehlmessungen nicht zu werten
int moisture_sensor_read_raw_median(uint8_t samples, uint16_t delay_ms);

/// Berechnet die Bodenfeuchtigkeit in Prozent (0 - 100%).
float moisture_sensor_read_percentage();

/// Gibt Feuchtigkeitslevel auf Serial Monitor aus
// void print_state();