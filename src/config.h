#pragma once
// ======================================================
// the only one place, where PINs und settings living
// all PIN changes only here!!!
// ======================================================

#define FW_VERSION  "0.4.0"   // firmware version, sent to the server (boot event, status)

// ============ PINs ==================
#define TRIG_PIN       12   // ultrasonic sensor: trigger
#define ECHO_PIN       14   // ultrasonic sensor: echo
#define LED_PIN        25   // LED-strip
#define MOISTURE_PIN_1 34   // moisture sensor 1, AO
#define MOISTURE_PIN_2 35   // moisture sensor 2, AO
#define PUMP_PIN       26   // relay module IN  - on PICO 15 Relais IN PIN
#define MOISTURE_POWER_PIN  27  // VCC of both moisture sensors (3.3 V from GPIO)
#define TANK_POWER_PIN      33  // breadboard only: VCC of HC-SR04 (no access to 3V3 pin)
#define DISPLAY_DC_PIN      16  // screen DC  (label RX2)
#define DISPLAY_RST_PIN     17  // screen RES (label TX2)
// screen SCL -> 18, SDA -> 23: default hardware SPI pins, not set in code

// ============ LED-strip ==============
#define TOTAL_LEDS     60   // whole 1m of strip
#define ACTIVE_LEDS    35   // how many diods are realy in use
// #define BRIGHTNESS     200  // ATTENTION: on a real use set to ~30
#define BRIGHTNESS     30   // real use 
#define MAX_MILLIAMPS  500  // hard power limit

// ============ Display ================
// 1.3" IPS 240x240, ST7789 (module GMT130-V1.0)
#define DISPLAY_ENABLED   1            // 0 = no screen connected
#define DISPLAY_SPI_HZ    27000000UL   // the ESP32 takes the next lower step: 80/3 = 26.7 MHz.
                                       // Was 40 MHz; slower is more robust against supply dips
#define DISPLAY_ROTATION  1            // 0..3: turn the picture in 90° steps (fit to the case).
                                       // 3: the screen is mounted turned 90° clockwise in the case
#define DISPLAY_FRAME_MS  100UL        // ~10 frames/s; one frame blocks loop() ~40 ms at 26.7 MHz

// Screen recovery: the pump inrush current (and the relay coil) can make the
// supply dip, the ST7789 controller resets and stays black (backlight still on).
// The screen is then restarted in software: RES pulse + init commands, non-blocking.
#define DISPLAY_REINIT_AFTER_PUMP_ON_MS  500UL     // restart this long after the pump switched on;
                                                   // no drawing at all before (inrush current)
#define DISPLAY_REINIT_AFTER_PUMP_OFF_MS 300UL     // restart this long after the pump switched off: a server
                                                   // call held back while pumping runs first (blocks up to 8 s)
#define DISPLAY_REINIT_INTERVAL_MS       180000UL  // and every 3 min a soft restart (no RES pulse, no blinking),
                                                   // in case a dip came from elsewhere
#define DISPLAY_RESET_LOW_MS             10UL      // RES held LOW
#define DISPLAY_RESET_WAIT_MS            120UL     // ST7789: after RES wait up to 120 ms before SLPOUT
#define DISPLAY_SLPOUT_WAIT_MS           10UL      // ST7789: wait >= 5 ms after SLPOUT before next commands

// ============ Water tank ==============
// These two numbers need to be
// calibrated using an actual tank (measured with a ruler)
#define TANK_DIST_FULL_CM  3.0f  // full tank distance
#define TANK_DIST_EMPTY_CM  10.2f // empty tank distance

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
#define WATER_DOSE_ML         50       // one portion without a profile (default)
#define MAX_WATER_ATTEMPTS    3        // portions in a row before ERROR (INTEGRATION: max 3)
#define PUMP_MAX_ON_MS        10000UL  // hard safety limit for one pump run
#define TANK_EMPTY_PERCENT    5        // at or below: pump is blocked (no dry running)

// Largest portion the pump can deliver inside PUMP_MAX_ON_MS (with margin).
// A bigger dose would always end in "pump ran too long".
#define DOSE_MAX_ML           150      // 150 ml / 23 ml/s = 6.5 s < 10 s
static_assert(DOSE_MAX_ML * 1000UL / PUMP_FLOW_ML_PER_S < PUMP_MAX_ON_MS,
              "DOSE_MAX_ML does not fit into PUMP_MAX_ON_MS");
static_assert(WATER_DOSE_ML <= DOSE_MAX_ML, "WATER_DOSE_ML is bigger than DOSE_MAX_ML");

// ============ Plant profile ===========
// Used until the server sends a profile (and if the stored one is invalid).
// Moisture values are the index 0..100.
#define DEFAULT_MOIST_MIN     30   // start watering below this
#define DEFAULT_MOIST_TARGET  50   // give portions until this is reached
#define DEFAULT_MOIST_MAX     70   // never pump at or above this
#define DEFAULT_MIN_PAUSE_H   0    // no pause between cycles without a profile
#define PROFILE_MAX_PAUSE_H   720  // INTEGRATION: min_pause_h 0..720
// min_pause_h is given in hours. In test mode an "hour" lasts one second.
#if WATER_TEST_TIMING
  #define PAUSE_UNIT_MS  1000UL
#else
  #define PAUSE_UNIT_MS  3600000UL
#endif

// ============ Fair demo plant (sim.h, demo/display_demo) ============
// One default plant for the demo, when no plant profile came from the server.
// Fensterblatt (Monstera deliciosa, id 9001) from server/plants.json, pot 1 l (bot default
// DEFAULT_POT_L), numbers as the server's buildProfile() would send them:
//   soil min/target/max 30/50/70 (soil_moisture)
//   dose  = 120 ml per litre x 1 l = 120 ml  (one portion, <= DOSE_MAX_ML)
//   pause = minimum_days 7 x 24 h x 0.5 (PAUSE_FACTOR) = 84 h  (84 s with WATER_TEST_TIMING)
#define DEMO_PLANT_ID        9001
#define DEMO_PLANT_NAME      "Fensterblatt"
#define DEMO_MOIST_MIN       30
#define DEMO_MOIST_TARGET    50
#define DEMO_MOIST_MAX       70
#define DEMO_DOSE_ML         120
#define DEMO_MIN_PAUSE_H     84

// ============ Wi-Fi / OTA =============
// SSID and passwords live in src/secrets.h (not in git)
#define OTA_HOSTNAME   "bloombot"   // -> bloombot.local
#define WIFI_RETRY_MS  15000UL      // next connection attempt if Wi-Fi is down

// ============ Telnet log ==============
// Log output + commands over Wi-Fi, see logger.h
#define TELNET_PORT      23
#define LOG_BUFFER_SIZE  1024   // text held back while the pump runs

// ============ Events =================
#define EVENT_QUEUE_SIZE  5     // events waiting for the server (Integration.md: up to 5)

// ============ Server connection ======
// URL, device id and key live in src/secrets.h
#define NET_POLL_DEFAULT_S      600     // status every 10 min, until the server says otherwise
#define NET_POLL_MIN_S          60      // poll_s from the server is only accepted in 60..3600
#define NET_POLL_MAX_S          3600
#define NET_CONNECT_TIMEOUT_MS  4000    // give up connecting after 4 s (LED freeze at most this long)
#define NET_TIMEOUT_MS          4000    // give up waiting for the answer after 4 s
#define NET_JSON_SIZE           768     // bytes for the status JSON
#define TANK_CAPACITY_ML        1700    // TBD: real tank volume, sent to the server for the water forecast
