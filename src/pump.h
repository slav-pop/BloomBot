#pragma once
#include "state.h"
// ============================================================
//  pump.h — watering state machine + relay (JQC-3FF-S-Z).
//  The ONLY module that switches the pump.
//  Writes 'state': waterPhase, needWater, pumpOn, waterAttempts, errorFlag.
//  Reads 'state': moisturePercent, tankPercent, waterRequest, errorResetRequest.
// ============================================================
void pumpBegin();
void pumpUpdate();
const char* waterPhaseName(WaterPhase p);   // for serial output