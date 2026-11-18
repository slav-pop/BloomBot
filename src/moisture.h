#pragma once
// ============================================================
//  moisture.h - two soil moisture sensors (resistive HD-38) in one pot.
//  Non-blocking: moistureRequest() starts a measurement, the result is
//  ready when state.moistureSeq has changed (~MOISTURE_POWER_ON_MS later).
//  Writes 'state': moistureRaw[], moistureSensorIndex[], moistureIndex,
//                  moistureSensorFail, moistureMismatch, moistureSeq.
// ============================================================
void moistureBegin();
void moistureUpdate();    // call every loop(), never blocks longer than ~10 ms
void moistureRequest();   // start a measurement (ignored if one is running)
bool moistureBusy();      // true while a measurement is running
