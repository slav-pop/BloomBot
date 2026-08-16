#pragma once
#include <Arduino.h>

/// Initialisiert die Wasserpumpe / das Relais.
/// \param pin GPIO-Pin des Pico (z.B. 15 für GP15)
/// \param activeLow Setze auf true, falls dein Relais bei LOW einschaltet (Standard bei 90% aller Relaismodule)
void water_pump_init(uint8_t pin, bool activeLow = true);

/// Nur benutzen, wenn keine Kontrolle notwendig ist, benutze statt dessen water_pump_try_water_plant
void water_pump_water_plant(long pumpTimeMS);

/// Bewässert kurz und kontrolliert ob Wasser ankommt, bevor richtiges bewässern statt findet
void water_pump_try_water_plant(float moisturePercent, long pumpTimeMS);

/// Pflanzen brauchen Milliliter, Pumpe arbeitet aber mit Millisekunden
int milliLiter_to_milliSeconds(int miliLiter);

/// Schaltet die Pumpe ein.
void water_pump_on();

/// Schaltet die Pumpe aus.
void water_pump_off();

/// Gibt zurück, ob die Pumpe aktuell läuft.
bool water_pump_is_on();
