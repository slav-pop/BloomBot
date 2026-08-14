#pragma once

// ======================================================
// leds.h - interface for otherss
// ======================================================

void ledsBegin();   // call once in setup()
void ledsUpdate();  // call every loop()-cycle; read state.ledState themself