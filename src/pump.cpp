#include <Arduino.h>
#include "config.h"
#include "pump.h"
#include "moisture.h"
#include "state.h"

// ============================================================
//  Watering state machine:
//
//  IDLE --(check time)--> soil check
//      soil ok  -> stay IDLE
//      soil dry -> PUMPING (or TANK_EMPTY if no water)
//  PUMPING --(dose pumped)--> SOAKING
//  SOAKING --(SOAK_MS)-----> soil check
//      soil ok                     -> IDLE
//      still dry, attempts left    -> PUMPING again
//      still dry, no attempts left -> ERROR
//  TANK_EMPTY --(tank refilled)--> IDLE, immediate soil check
//  ERROR --(manual reset)--> IDLE, immediate soil check
// ============================================================

static unsigned long phaseStartedAt = 0;   // when the current phase began
static unsigned long pumpStartedAt  = 0;   // when the pump was switched on
static unsigned long lastCheckAt    = 0;   // last soil check
static unsigned long checkDelay     = FIRST_CHECK_DELAY_MS;  // time until next check
static unsigned long pumpTimeMs     = 0;   // how long to pump this time
static bool locked = false;                // pumpLock(): never switch on again until reboot

// soil check is non-blocking: request a measurement, evaluate it when ready
static bool soilPending = false;           // waiting for a moisture result
static bool soilAfterWatering = false;     // the pending check follows a portion
static uint32_t soilSeq = 0;               // state.moistureSeq when requested

// ---- the only place where the relay pin is written ----
static void water_pump_on() {
    if (locked) return;
    digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
    state.pumpOn = true;
    pumpStartedAt = millis();
}

static void water_pump_off() {
    digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
    state.pumpOn = false;
}

// ---- ml -> ms, using the measured pump flow ----
static unsigned long milliLiter_to_milliSeconds(int milliLiter) {
    return (unsigned long)milliLiter * 1000UL / PUMP_FLOW_ML_PER_S;
}

const char* waterPhaseName(WaterPhase p) {
    switch (p) {
        case PHASE_IDLE:       return "IDLE";
        case PHASE_PUMPING:    return "PUMPING";
        case PHASE_SOAKING:    return "SOAKING";
        case PHASE_TANK_EMPTY: return "TANK_EMPTY";
        case PHASE_ERROR:      return "ERROR";
    }
    return "?";
}

static void enterPhase(WaterPhase p) {
    soilPending = false;                   // a phase change cancels a pending check
    state.waterPhase = p;
    phaseStartedAt = millis();
    Serial.print("WATER: -> ");
    Serial.println(waterPhaseName(p));
}

static void goError(const char* reason) {
    water_pump_off();
    state.errorFlag = true;
    Serial.print("WATER: ERROR - ");
    Serial.println(reason);
    enterPhase(PHASE_ERROR);
}

// -1 (no tank data) also counts as "no water" - better safe
static bool tankHasWater() {
    return state.tankPercent > TANK_EMPTY_PERCENT;
}

// ---- switch the pump on, if there is water ----
static void startPumping() {
    if (!tankHasWater()) {
        enterPhase(PHASE_TANK_EMPTY);
        return;
    }
    state.waterAttempts++;
    pumpTimeMs = milliLiter_to_milliSeconds(WATER_DOSE_ML);
    water_pump_on();
    enterPhase(PHASE_PUMPING);
}

// ---- start a soil measurement; checkSoil() runs when the result is there ----
static void requestSoilCheck(bool afterWatering) {
    soilSeq = state.moistureSeq;
    soilAfterWatering = afterWatering;
    soilPending = true;
    lastCheckAt = millis();
    checkDelay = MOISTURE_CHECK_INTERVAL_MS;
    moistureRequest();
}

// true once, when the requested measurement has finished
static bool soilResultReady() {
    if (!soilPending || state.moistureSeq == soilSeq) return false;
    soilPending = false;
    return true;
}

// ---- the soil was measured: decide what to do ----
static void checkSoil(bool afterWatering) {
    Serial.print("WATER: soil ");
    Serial.print(state.moistureIndex);
    Serial.print("/100 (sensors ");
    Serial.print(state.moistureSensorIndex[0]);
    Serial.print(" / ");
    Serial.print(state.moistureSensorIndex[1]);
    Serial.print("), threshold ");
    Serial.println((int)THRESHOLD_DRY);

    // Both sensors failed: do not water blind. Back to IDLE, the next
    // regular check tries again (LED shows the error via moistureSensorFail).
    if (state.moistureIndex < 0) {
        Serial.println("WATER: no data from both moisture sensors - no watering");
        state.needWater = false;
        state.waterAttempts = 0;
        if (state.waterPhase != PHASE_IDLE) enterPhase(PHASE_IDLE);
        return;
    }

    if (state.moistureIndex >= THRESHOLD_DRY) {   // soil ok
        state.needWater = false;
        state.waterAttempts = 0;
        if (state.waterPhase != PHASE_IDLE) enterPhase(PHASE_IDLE);
        return;
    }

    state.needWater = true;                         // soil dry
    if (!afterWatering) state.waterAttempts = 0;    // a new watering cycle begins

    if (state.waterAttempts < MAX_WATER_ATTEMPTS) {
        startPumping();
    } else {
        goError("soil still dry after all attempts (hose? sensor? pump?)");
    }
}

// ============================================================

void pumpBegin() {
    pinMode(PUMP_PIN, OUTPUT);
    water_pump_off();   // relay off right after boot
    phaseStartedAt = millis();
    lastCheckAt = millis();
}

void pumpLock() {
    locked = true;
    water_pump_off();
}

void pumpUpdate() {
    if (locked) return;   // OTA update running: nothing happens until reboot

    unsigned long now = millis();
    unsigned long inPhase = now - phaseStartedAt;

    // ---- hard safety, independent of the phase ----
    if (state.pumpOn && now - pumpStartedAt > PUMP_MAX_ON_MS) {
        goError("pump ran too long");
    }

    switch (state.waterPhase) {

        case PHASE_IDLE:
            if (state.waterRequest) {              // manual "water now"
                state.needWater = true;
                state.waterAttempts = 0;
                startPumping();
            } else if (soilResultReady()) {
                checkSoil(soilAfterWatering);
            } else if (!soilPending && now - lastCheckAt >= checkDelay) {
                requestSoilCheck(false);
            }
            break;

        case PHASE_PUMPING:
            if (!tankHasWater()) {                 // tank ran empty while pumping
                water_pump_off();
                enterPhase(PHASE_TANK_EMPTY);
            } else if (inPhase >= pumpTimeMs) {    // dose pumped
                water_pump_off();
                enterPhase(PHASE_SOAKING);
            }
            break;

        case PHASE_SOAKING:
            if (soilResultReady()) checkSoil(true);
            else if (!soilPending && inPhase >= SOAK_MS) requestSoilCheck(true);
            break;

        case PHASE_TANK_EMPTY:
            if (state.tankPercent > TANK_LOW_OFF_PERCENT) {   // refilled
                checkDelay = 0;                    // check the soil right away
                enterPhase(PHASE_IDLE);
            }
            break;

        case PHASE_ERROR:
            if (state.errorResetRequest) {
                state.errorFlag = false;
                state.needWater = false;
                state.waterAttempts = 0;
                checkDelay = 0;                    // check the soil right away
                enterPhase(PHASE_IDLE);
            }
            break;
    }

    // requests are one-shot: handled above, or ignored in other phases
    state.waterRequest = false;
    state.errorResetRequest = false;
}