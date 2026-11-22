#include <Arduino.h>
#include "config.h"
#include "pump.h"
#include "moisture.h"
#include "state.h"
#include "logger.h"
#include "events.h"

// ============================================================
//  Watering state machine:
//
//  All numbers come from state.profile (moistMin/Target/Max, doseMl,
//  minPauseH), see profile.cpp.
//
//  IDLE --(check time)--> soil check
//      index >= moistMin            -> stay IDLE
//      dry, but pause not over yet  -> stay IDLE (needWater = true)
//      dry                          -> PUMPING (or TANK_EMPTY if no water)
//  PUMPING --(dose pumped)--> SOAKING
//  SOAKING --(SOAK_MS)-----> soil check
//      index >= moistTarget          -> cycle done, IDLE, pause starts
//      below target, attempts left   -> PUMPING again
//      no attempts left, >= moistMin -> cycle done (target not reached)
//      no attempts left, < moistMin  -> ERROR (still dry)
//  TANK_EMPTY --(tank refilled)--> IDLE, immediate soil check
//  ERROR --(manual reset)--> IDLE, immediate soil check
//  In TANK_EMPTY and ERROR the soil is still measured every
//  MOISTURE_CHECK_INTERVAL_MS, only for the screen/server (no watering decision).
//
//  The pump never starts automatically at index >= moistMax.
//  Serial 'w' (manual test) ignores moistMax and the pause, not the tank.
//  The pause applies between cycles since boot: the ESP32 has no clock,
//  so the first cycle after power on is allowed right away.
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

// pause between watering cycles (profile.minPauseH)
static bool cycleDoneSinceBoot = false;    // false: first cycle after boot needs no pause
static unsigned long lastCycleEndAt = 0;
static uint32_t cycleMl = 0;               // ml pumped in the current cycle (for the "watered" event)

// ---- the only place where the relay pin is written ----
static void water_pump_on() {
    if (locked) return;
    digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
    state.pumpOn = true;
    pumpStartedAt = millis();
}

static void water_pump_off() {
    digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
    if (state.pumpOn) {
        // count the water that was really pumped (a run can end early,
        // e.g. tank empty): run time x measured flow
        unsigned long ms = millis() - pumpStartedAt;
        uint16_t ml = (uint16_t)((ms * PUMP_FLOW_ML_PER_S + 500UL) / 1000UL);   // rounded
        state.totalPumpedMl += ml;
        state.lastDoseMl = ml;
        state.lastWateredAt = millis();
        state.wateredSinceBoot = true;
        cycleMl += ml;
    }
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
    if (p == PHASE_TANK_EMPTY && state.waterPhase != PHASE_TANK_EMPTY) {
        eventPush("tank_empty");           // the plant needs water, but there is none
    }
    state.waterPhase = p;
    phaseStartedAt = millis();
    Log.print("WATER: -> ");
    Log.println(waterPhaseName(p));
}

// code = short reason for the server ("still_dry", "pump_timeout"),
// reason = long text for the log
static void goError(const char* code, const char* reason) {
    water_pump_off();
    state.errorFlag = true;
    state.errorReason = code;
    eventPush("error", 0, 0, code);
    Log.print("WATER: ERROR - ");
    Log.println(reason);
    enterPhase(PHASE_ERROR);
}

// -1 (no tank data) also counts as "no water" - better safe
static bool tankHasWater() {
    return state.tankPercent > TANK_EMPTY_PERCENT;
}

// ---- switch the pump on, if there is water (and the soil is not too wet) ----
static void startPumping(bool manual) {
    if (!tankHasWater()) {
        enterPhase(PHASE_TANK_EMPTY);
        return;
    }
    if (!manual && state.moistureIndex >= state.profile.moistMax) {
        Log.println("WATER: index >= moist_max - pump stays off");
        if (state.waterPhase != PHASE_IDLE) enterPhase(PHASE_IDLE);
        return;
    }
    state.waterAttempts++;
    pumpTimeMs = milliLiter_to_milliSeconds(state.profile.doseMl);
    water_pump_on();
    enterPhase(PHASE_PUMPING);
}

static unsigned long pauseMs() {
    return (unsigned long)state.profile.minPauseH * PAUSE_UNIT_MS;
}

static bool pauseRunning() {
    return cycleDoneSinceBoot && millis() - lastCycleEndAt < pauseMs();
}

// ---- a watering cycle is over: back to IDLE, the pause starts ----
static void finishCycle() {
    if (cycleMl > 0) eventPush("watered", cycleMl, state.moistureIndex);
    cycleMl = 0;
    state.needWater = false;
    state.waterAttempts = 0;
    cycleDoneSinceBoot = true;
    lastCycleEndAt = millis();
    if (state.waterPhase != PHASE_IDLE) enterPhase(PHASE_IDLE);
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

// ---- TANK_EMPTY / ERROR: keep measuring the soil for the screen and the server ----
// Same interval as in IDLE (no extra wear on the probes), but the result
// is NOT evaluated: no watering decision is made in these phases.
static void displayOnlySoilCheck(unsigned long now) {
    if (moistureBusy() || now - lastCheckAt < MOISTURE_CHECK_INTERVAL_MS) return;
    lastCheckAt = now;
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
    const Profile& p = state.profile;
    Log.print("WATER: soil ");
    Log.print(state.moistureIndex);
    Log.print("/100 (sensors ");
    Log.print(state.moistureSensorIndex[0]);
    Log.print(" / ");
    Log.print(state.moistureSensorIndex[1]);
    Log.print("), min/target/max ");
    Log.print(p.moistMin);
    Log.print("/");
    Log.print(p.moistTarget);
    Log.print("/");
    Log.println(p.moistMax);

    // Both sensors failed: do not water blind. Back to IDLE, the next
    // regular check tries again (LED shows the error via moistureSensorFail).
    if (state.moistureIndex < 0) {
        Log.println("WATER: no data from both moisture sensors - no watering");
        state.needWater = false;
        state.waterAttempts = 0;
        if (state.waterPhase != PHASE_IDLE) enterPhase(PHASE_IDLE);
        return;
    }

    if (!afterWatering) {
        // ---- regular check: start a new cycle? ----
        if (state.moistureIndex >= p.moistMin) {     // soil ok
            state.needWater = false;
            state.waterAttempts = 0;
            if (state.waterPhase != PHASE_IDLE) enterPhase(PHASE_IDLE);
            return;
        }
        state.needWater = true;                      // soil dry
        if (pauseRunning()) {
            // dry again so soon after a cycle: rather sensor drift than a thirsty
            // plant -> wait until the pause is over
            Log.print("WATER: dry, but pause running, ");
            Log.print((pauseMs() - (millis() - lastCycleEndAt)) / 1000UL);
            Log.println(" s left");
            return;
        }
        state.waterAttempts = 0;                     // a new watering cycle begins
        cycleMl = 0;
        startPumping(false);
        return;
    }

    // ---- after a portion: continue the cycle? ----
    if (state.moistureIndex >= p.moistTarget) {      // target reached
        finishCycle();
        return;
    }
    if (state.waterAttempts < MAX_WATER_ATTEMPTS) {
        startPumping(false);
    } else if (state.moistureIndex >= p.moistMin) {
        Log.println("WATER: target not reached, but soil no longer dry - cycle done");
        finishCycle();
    } else {
        goError("still_dry", "soil still dry after all attempts (hose? sensor? pump?)");
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
        goError("pump_timeout", "pump ran too long");
    }

    switch (state.waterPhase) {

        case PHASE_IDLE:
            if (state.waterRequest) {              // manual "water now"
                state.needWater = true;
                state.waterAttempts = 0;
                cycleMl = 0;
                startPumping(true);                // ignores moistMax and the pause
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
            } else {
                displayOnlySoilCheck(now);
            }
            break;

        case PHASE_ERROR:
            if (state.errorResetRequest) {
                state.errorFlag = false;
                eventPush("error_cleared", 0, 0, state.errorReason);
                state.errorReason = nullptr;
                state.needWater = false;
                state.waterAttempts = 0;
                checkDelay = 0;                    // check the soil right away
                enterPhase(PHASE_IDLE);
            } else {
                displayOnlySoilCheck(now);
            }
            break;
    }

    // requests are one-shot: handled above, or ignored in other phases
    state.waterRequest = false;
    state.errorResetRequest = false;
}