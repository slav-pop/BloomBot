#pragma once
// ======================================================
// the only one place, where PINs und settings living
// all PIN changes only here!!!
// ======================================================

// ============ PINs ==================
#define TRIG_PIN       12   // ultrasonic sensor: trigger
#define ECHO_PIN       14   // ultrasonic sensor: echo
#define LED_PIN        25   // LED-strip
#define MOISTURE_PIN   34   // moisture sensor  - on PICO 26 GP26 (ADC0)
#define PUMP_PIN       26   // relay module IN  - on PICO 15 Relais IN PIN
#define MOISTURE_POWER_PIN  27  // VCC of moisture sensor (3.3 V from GPIO)
#define TANK_POWER_PIN      33  // breadboard only: VCC of HC-SR04 (no access to 3V3 pin)

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
#define TANK_FAST_WINDOW_MS  8000UL   // stay fast 8s after pump of


// ============ Moisture ===============
#define DRY_VAL 4095  // sensor in air (ESP32 ADC saturates at ~3.1 V)
#define WET_VAL 1500  // sensor in a glass of water (measured 1100-1650, drifts up)
#define THRESHOLD_DRY 30.0f  // randomly picked value for now -> is there a better one or depending on plant ?

// ============ Relay ==================
// true  = relay switches ON when IN is LOW
// false = relay switches ON when IN is HIGH (jumper "H")
#define RELAY_ACTIVE_LOW  false

// ============ Watering ===============
// 1 = short times for testing on the table, 0 = real plant
#define WATER_TEST_TIMING  1

#if WATER_TEST_TIMING
  #define MOISTURE_CHECK_INTERVAL_MS  10000UL    // 10 s
  #define SOAK_MS                     5000UL    // 5 s
#else
  #define MOISTURE_CHECK_INTERVAL_MS  3600000UL  // 1 h
  #define SOAK_MS                     600000UL   // 10 min, adjust in practice
#endif

#define FIRST_CHECK_DELAY_MS  3000UL   // first soil check after boot (tank needs data first)
#define PUMP_FLOW_ML_PER_S    23       // measured: ~47 ml in 2 s
#define WATER_DOSE_ML         50       // one portion. TODO: from plant DB
#define MAX_WATER_ATTEMPTS    2        // portions in a row before ERROR
#define PUMP_MAX_ON_MS        10000UL  // hard safety limit for one pump run
#define TANK_EMPTY_PERCENT    5        // at or below: pump is blocked (no dry running)

// ============ Wi-Fi / OTA =============
// SSID and passwords live in src/secrets.h (not in git)
#define OTA_HOSTNAME   "bloombot"   // -> bloombot.local
#define WIFI_RETRY_MS  15000UL      // next connection attempt if Wi-Fi is down
