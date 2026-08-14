#pragma once
// ============================================================
//  water_tank.h — tank water level sensor module.
//  Stores the results in 'state': tankDistanceCm, tankPercent, tankLow.
// ============================================================
void tankBegin();
void tankUpdate();