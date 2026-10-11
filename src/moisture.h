#pragma once
// ============================================================
//  moisture.h — soil moisture sensor module (resistive HD38).
//  Reads the sensor and stores the results in 'state':
//  moistureRaw, needWater.
//  TODO: read interval (non-blocking), ESP32 calibration (12 bit).
// ============================================================
void moistureBegin();
void moistureUpdate();