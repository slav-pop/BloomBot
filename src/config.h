#pragma once
// ======================================================
// the only one place, where PINs und settings living
// all PIN changes only here!!!
// ======================================================

// ============ PINs ==================
#define TRIG_PIN       12   // ultrasonic sensor: trigger
#define ECHO_PIN       14   // ultrasonic sensor: echo
#define LED_PIN        25   // LED-strip
#define MOISTURE_PIN_1 34   // moisture sensor 1, AO
#define MOISTURE_PIN_2 35   // moisture sensor 2, AO
#define PUMP_PIN       26   // relay module IN  - on PICO 15 Relais IN PIN
#define MOISTURE_POWER_PIN  27  // VCC of both moisture sensors (3.3 V from GPIO)
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

// Failed readings in a row before the tank data counts as invalid.
// slow mode: 5 x 2 s = 10 s, while pumping: 5 x 0.3 s = 1.5 s
#define TANK_MAX_FAILS        5

// ============ Intervals ===============
#define TANK_READ_SLOW_MS    2000UL   // idle: rare checks
#define TANK_READ_FAST_MS    300UL    // watering: react quickly
#define TANK_FAST_WINDOW_MS  8000UL   // stay fast 8s after pump of


// ============ Moisture ===============
// Calibration per sensor (the two modules differ), with the current
// MOISTURE_POWER_ON_MS. Moisture is a relative index 0..100, NOT physical %
// (INTEGRATION): 0 = dry soil, 100 = freshly watered and drained soil.
// Water is NOT a good reference:
// in a glass the probe reads ~160-600 and drifts up by itself (electrolysis).
// TODO: calibrate in the real pot: DRY = dry soil, WET = after watering
#define DRY_VAL_1 4095  // sensor 1 in air (ESP32 ADC saturates at ~3.1 V)
#define WET_VAL_1 1500  // sensor 1 in a glass of water
#define DRY_VAL_2 4095  // sensor 2 in air
#define WET_VAL_2 1500  // sensor 2 in a glass of water
#define THRESHOLD_DRY 30.0f  // randomly picked value for now -> is there a better one or depending on plant ?
// The sensors get power only while measuring: less corrosion of the probes
#define MOISTURE_POWER_ON_MS  300  // settle time after power on (30 ms was too short: dry read ~2700)
#define MOISTURE_SENSORS      2    // two sensors in one pot
#define MOISTURE_SAMPLES      5    // samples per sensor, the median is used
// Plausibility check: a failed sensor must not look like "dry soil".
// TODO: verify by unplugging the AO wire of one sensor (before final assembly)
#define MOISTURE_MAX_SPREAD    400 // max - min of the samples; more = loose wire / floating pin
#define MOISTURE_MISMATCH_DIFF 25  // sensors differ by more index points -> moistureMismatch (TBD)

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
#define MAX_WATER_ATTEMPTS    3        // portions in a row before ERROR (INTEGRATION: max 3)
#define PUMP_MAX_ON_MS        10000UL  // hard safety limit for one pump run
#define TANK_EMPTY_PERCENT    5        // at or below: pump is blocked (no dry running)

// Largest portion the pump can deliver inside PUMP_MAX_ON_MS (with margin).
// A bigger dose would always end in "pump ran too long".
#define DOSE_MAX_ML           150      // 150 ml / 23 ml/s = 6.5 s < 10 s
static_assert(DOSE_MAX_ML * 1000UL / PUMP_FLOW_ML_PER_S < PUMP_MAX_ON_MS,
              "DOSE_MAX_ML does not fit into PUMP_MAX_ON_MS");
static_assert(WATER_DOSE_ML <= DOSE_MAX_ML, "WATER_DOSE_ML is bigger than DOSE_MAX_ML");

// ============ Wi-Fi / OTA =============
// SSID and passwords live in src/secrets.h (not in git)
#define OTA_HOSTNAME   "bloombot"   // -> bloombot.local
#define WIFI_RETRY_MS  15000UL      // next connection attempt if Wi-Fi is down
