#pragma once
#include <Arduino.h>
// ============================================================
//  sim.h - fair demo: simulated soil and tank values
//
//  Set from the web page (scripts/device-bridge.mjs) or by hand over telnet.
//  ONLY the two sensor values are simulated. Everything else stays real:
//  watering logic, pump, LED strip, screen and the events to the server
//  (Telegram). At the fair the hose goes from the pot into an empty glass.
//
//  Commands (one character):
//    -  +   soil index -10 / +10        D   dry soil (below moist_min)
//    [  ]   tank level -10 / +10 %      L   low tank (below the "low" limit)
//    V      tank full again             H   hose off / on (water does not reach the soil)
//    o      simulation off: real sensors again
//    G      the glass was emptied: water budget starts again (terminal only, not on the web page)
//    j      one machine-readable status line "STATE {...}" for the web page
//
//  Safety: the simulated tank can only show LESS water than the real sensor
//  measures, never more - the pump can not run dry because of the demo.
//  A failed real tank sensor still blocks the pump.
//  Glass budget: once the demo was used (until reboot, also after 'o'), after 300 ml
//  in the glass the tank counts as empty (stops visitors from pumping the whole tank
//  next to the electronics). Only 'G' (terminal) or a reboot starts a new glass.
//  The simulated soil also hides a broken real soil sensor - demo only.
// ============================================================

bool simHandleCommand(char c);   // true = c was a simulation command (handled)
void simUpdate();                // call every loop() after pumpUpdate()
int  simSoil();                  // simulated soil index 0..100, -1 = real sensors
int  simTankLimit();             // simulated tank level 0..100, -1 = real sensor only
