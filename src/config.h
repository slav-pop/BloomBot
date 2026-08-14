#pragma once
// ======================================================
// the only one place, where PINs und settings living
// all PIN changes only here!!!
// ======================================================

// ============ PINs ==================
#define TRIG_PIN       12   // ultrasonic sensor: trigger
#define ECHO_PIN       14   // ultrasonic sensor: echo
#define LED_PIN        25   // LED-strip
#define MOISTURE_PIN   34   // moisture sensor
#define PUMP_PIN       26   // MOSFET module

// ============ LED-strip ==============
#define TOTAL_LEDS     60   // whole 1m of strip
#define ACTIVE_LEDS    15   // how many diods are realy in use
// #define BRIGHTNESS     200  // ATTENTION: on a real use set to ~30
#define BRIGHTNESS     30   // real use 
#define MAX_MILLIAMPS  500  // hard power limit

// ============ Water tank ==============
// These two numbers need to be
// calibrated using an actual tank (measured with a ruler)
#define TANK_DIST_FULL_CM  3.0f  // full tank distance
#define TANK_DIST_EMPTY_CM  20.0f // empty tank distance

// Hysteresis, so that the "low water" status doesn't keep
// flickering back and forth at the threshold
#define TANK_LOW_ON_PERCENT   20 // turn on "low water", when less then 20%
#define TANK_LOW_OFF_PERCENT  25 // turn off only when it's above 25% again

// ============ Intervals ===============
#define TANK_READ_SLOW_MS    2000UL   // idle: rare checks
#define TANK_READ_FAST_MS    300UL    // watering: react quickly
#define TANK_FAST_WINDOW_MS  8000UL   // stay fast 8s after pump off