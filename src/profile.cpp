#include <Arduino.h>
#include <Preferences.h>   // NVS storage of the ESP32 (part of the Arduino core)
#include <string.h>
#include "config.h"
#include "state.h"
#include "logger.h"
#include "profile.h"

// ============================================================
//  profile.cpp - checking, saving and loading the plant profile
//
//  How it is saved:
//    NVS is a small part of the flash memory that keeps its content
//    without power. It works like a list of "key = value" entries,
//    grouped in a namespace (like a folder). We use the namespace
//    "profile" with two keys:
//      "schema" - the format number PROFILE_SCHEMA (see below)
//      "data"   - the whole struct Profile as one block of raw bytes,
//                 exactly as it lies in memory
//
//  Why the format number (PROFILE_SCHEMA):
//    The raw bytes only make sense for the struct layout they w ere
//    written with. If a field is added to struct Profile, an old block
//    would be read with the new layout and the numbers would land in
//    the wrong fields (garbage thresholds = dangerous for the pump).
//    So: whenever struct Profile changes, increase PROFILE_SCHEMA.
//    After such a firmware update the old block is ignored, the defaults
//    are used, and the device reports profile_ver 0 to the server, which
//    then simply sends the current profile again.
//    (profile_ver is else: the version of the CONTENT, i.e.
//    which plant settings; it changes with every choice in the bot.)
// ============================================================

#define PROFILE_SCHEMA  1                          // increase when struct Profile changes
static const char* NVS_NAMESPACE = "profile";      // the "folder" in NVS

// ---- default values (used until the server sends a profile) ----
void profileDefaults(Profile& p) {
    memset(&p, 0, sizeof(p));               // first set every byte to 0: no leftovers
    p.ver = 0;                              // 0 = no profile from the server
    p.plantId = -1;
    // Text fields have a fixed size (name: 32 bytes). strncpy copies at most
    // size - 1 characters, so the last byte stays 0 = the end mark of the text
    strncpy(p.name, "default", sizeof(p.name) - 1);
    strncpy(p.mode, "normal", sizeof(p.mode) - 1);
    p.moistMin = DEFAULT_MOIST_MIN;
    p.moistTarget = DEFAULT_MOIST_TARGET;
    p.moistMax = DEFAULT_MOIST_MAX;
    p.doseMl = WATER_DOSE_ML;
    p.minPauseH = DEFAULT_MIN_PAUSE_H;
}

// ---- range check: each line is one rule. The first broken rule ends the
// ---- function and returns its text; if no rule is broken -> nullptr ----
const char* profileCheck(const Profile& p) {
    if (p.ver < 1)                      return "profile_ver < 1";
    if (p.moistMin > 100)               return "moist_min > 100";
    if (p.moistTarget <= p.moistMin)    return "moist_target <= moist_min";
    if (p.moistMax <= p.moistTarget)    return "moist_max <= moist_target";
    if (p.moistMax > 100)               return "moist_max > 100";
    if (p.doseMl < 10)                  return "dose_ml < 10";
    if (p.doseMl > DOSE_MAX_ML)         return "dose_ml > DOSE_MAX_ML (pump time limit)";
    if (p.minPauseH > PROFILE_MAX_PAUSE_H) return "min_pause_h > 720";
    return nullptr;
}

// ---- take a new profile into use: check -> save in flash -> use ----
bool profileApply(const Profile& p, const char** reason) {
    const char* err = profileCheck(p);
    if (err) {                              // err is not nullptr = a rule is broken
        if (reason) *reason = err;
        Log.print("PROFILE: rejected - ");
        Log.println(err);
        return false;                       // the old profile stays untouched
    }

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);      // open the "folder", false = for writing
    prefs.putUChar("schema", PROFILE_SCHEMA);   // which struct layout these bytes have
    prefs.putBytes("data", &p, sizeof(p));      // the whole profile: sizeof(p) bytes from address &p
    prefs.end();                            // close

    state.profile = p;                      // from now on the pump uses the new numbers
    Log.print("PROFILE: applied and saved, ver ");
    Log.println(p.ver);
    return true;
}

// ---- at boot: load the saved profile, otherwise keep the defaults ----
void profileBegin() {
    profileDefaults(state.profile);         // defaults first: they stay if loading fails

    Profile stored;
    Preferences prefs;
    // Four conditions:
    //   1. the "folder" opens (true = read only; on the very first boot it
    //      does not exist yet)
    //   2. the saved format number is the current one (see top of file)
    //   3. the saved block has exactly the size of today's struct Profile -
    //      a second safety net in case PROFILE_SCHEMA was not increased
    //   4. reading the block returns all its bytes
    bool found = prefs.begin(NVS_NAMESPACE, true)
              && prefs.getUChar("schema", 0) == PROFILE_SCHEMA
              && prefs.getBytesLength("data") == sizeof(stored)
              && prefs.getBytes("data", &stored, sizeof(stored)) == sizeof(stored);
    prefs.end();

    // Also check the loaded numbers again: flash content is never fully trusted
    if (found && profileCheck(stored) == nullptr) {
        // A text in C/C++ ends with a 0 byte ('\0'); print() reads until it.
        // If flash were damaged and that 0 missing, print() would run past
        // the field. Setting the last byte to 0 makes sure the text ends.
        stored.name[sizeof(stored.name) - 1] = '\0';
        stored.mode[sizeof(stored.mode) - 1] = '\0';
        state.profile = stored;
        Log.print("PROFILE: loaded from flash, ver ");
        Log.println(stored.ver);
    } else {
        Log.println("PROFILE: none stored, using defaults");
    }
}

// ---- serial 'p': print the profile in use ----
void profilePrint() {
    const Profile& p = state.profile;
    Log.print("PROFILE: ver ");
    Log.print(p.ver);
    Log.print(p.ver == 0 ? " (defaults)" : "");
    Log.print(", plant ");
    Log.print(p.plantId);
    Log.print(" \"");
    Log.print(p.name);
    Log.print("\", mode ");
    Log.print(p.mode);
    Log.print(", moisture min/target/max ");
    Log.print(p.moistMin);
    Log.print("/");
    Log.print(p.moistTarget);
    Log.print("/");
    Log.print(p.moistMax);
    Log.print(", dose ");
    Log.print(p.doseMl);
    Log.print(" ml, min pause ");
    Log.print(p.minPauseH);
    Log.println(WATER_TEST_TIMING ? " h (TEST: counted as seconds)" : " h");
}
