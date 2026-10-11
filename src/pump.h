#pragma once
// ============================================================
//  pump.h — water pump module (relay module JQC-3FF-S-Z).
//  The ONLY module that switches the relay.
//  Will store the pump status in 'state': pumpOn.
//  TODO: watering state machine (pumpUpdate is disabled for now).
// ============================================================
void pumpBegin();
void pumpUpdate();