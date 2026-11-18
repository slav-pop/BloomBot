#pragma once
// ============================================================
//  display.h - 1.3" IPS screen 240x240 (ST7789, module GMT130-V1.0)
//  Shows an animated flower whose "mood" follows state.ledStatus,
//  the plant name on top and a data line (soil index, tank level) below.
//  Reads 'state': ledStatus, profile, moistureIndex, tankPercent,
//                 tankSensorFail, moistureSensorFail, errorReason.
//  Writes nothing.
// ============================================================
void displayBegin();    // call once in setup()
void displayUpdate();   // call every loop(); draws a frame every DISPLAY_FRAME_MS
void displayPrintStats(); // command 'f': frame time (last / max) and free memory
