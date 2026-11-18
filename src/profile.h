#pragma once
#include "state.h"   // struct Profile is defined there

// ============================================================
//  profile.h - plant profile: the numbers the pump works with
//
//  What a profile is:
//    When a plant is chosen in the bot, the server sends a few numbers:
//    start watering below moistMin, give portions until moistTarget,
//    never pump above moistMax, portion size doseMl, minimum pause
//    between two watering cycles minPauseH.
//
//  Where it lives:
//    - state.profile   : the profile in use (pump.cpp reads it)
//    - NVS (flash)     : a saved copy, survives reboot and power loss
//  Without any profile from the server, defaults from config.h are used,
//  so the device also works with no Wi-Fi and no server at all.
//
//  This file is the "table of contents": it only lists the functions.
//  What they do inside is in profile.cpp.
// ============================================================

// Called once in setup(). Puts the defaults into state.profile, then
// tries to load the saved profile from flash and uses it if it is valid.
void profileBegin();

// Fills the given profile with the default values from config.h.
// "Profile& p" = the function gets the caller's own variable (a reference),
// not a copy, so it can write into it:
//     Profile t;  profileDefaults(t);   // t is now filled
void profileDefaults(Profile& p);

// Checks that all numbers are within the allowed ranges
// (e.g. min < target < max <= 100, portion not bigger than DOSE_MAX_ML).
// Returns nullptr ("nothing", = no error) if everything is OK,
// otherwise a short text with the reason, e.g. "moist_max > 100".
// "const Profile& p" = reads the caller's profile, does not change it.
const char* profileCheck(const Profile& p);

// Takes a new profile into use:
//   1. checks it (profileCheck)
//   2. saves it in flash
//   3. copies it to state.profile - from now on the pump uses it
// Returns true if accepted. If rejected, it returns false, the old profile
// stays in use, and the reason text is written to *reason (pass nullptr
// if you don't need the reason). Later net.cpp sends this reason to the
// server as the event "profile_rejected".
bool profileApply(const Profile& p, const char** reason);

// Fair demo (sim.h): uses the profile like profileApply, but ONLY in RAM - nothing
// is written to flash, a reboot brings the saved profile (or the defaults) back.
bool profileUseDemo(const Profile& p, const char** reason);

// Prints the profile in use to Serial (serial command 'p').
void profilePrint();

// Forgets the profile: erases it from flash and goes back to the defaults.
// Used when the device is reset in the bot ("Gerät zurücksetzen"): the server
// then sends the command "factory_reset". Also serial command 'F'.
void profileFactoryReset();
