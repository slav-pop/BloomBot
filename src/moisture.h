#pragma once
// ============================================================
//  moisture.h — soil moisture sensor module (resistive HD38).
//  Measures only when asked (by pump.cpp or serial 'm').
//  Writes 'state': moistureRaw, moisturePercent.
// ============================================================
void moistureBegin();
void moistureMeasure();   // one measurement, takes ~5 ms