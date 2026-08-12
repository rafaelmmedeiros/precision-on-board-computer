#pragma once

#include <Arduino.h>

// GpsModule — NEO-8M driver: UART pump, NMEA parsing, and a self-validating
// UBX configuration handshake.
//
// Wiring (see CLAUDE.md §4.2):
//   GPS TX -> GPIO 39 (ESP32 RX, input-only)
//   GPS RX <- GPIO 15 (ESP32 TX, strapping pin but safe as output post-boot)
//   GPS VCC on the ignition-switched 3.3V rail, so the module is UNPOWERED
//   whenever the key is off. That is why the configuration handshake cannot
//   happen once in setup() — see the link state machine below.
//
// Tick contract: gpsTick() must be called once per main-loop iteration while
// the OBC is ACTIVE. It is non-blocking — each call performs at most one
// handshake action and drains whatever bytes are waiting, so it always fits
// inside the 100 ms frame budget.

// Link / configuration state. Exposed so the screen can show the handshake
// without a serial monitor attached.
//
//   ABSENT      — no valid sentence; retries the baud probe periodically.
//   PROBING     — hunting for the module's current baud rate.
//   VERIFYING   — polling the receiver's live config and comparing it.
//   CONFIGURING — applying only the settings that did not match, then saving.
//   READY       — configuration confirmed; normal NMEA operation.
//
// READY -> ABSENT on sentence starvation is what makes a module swap
// self-healing: the replacement is re-verified on its first appearance,
// without the firmware ever needing to know a swap happened.
enum class GpsLink : uint8_t { ABSENT, PROBING, VERIFYING, CONFIGURING, READY };

void gpsInit();    // opens Serial1 only — the handshake lives in gpsTick()
void gpsTick();    // pump bytes, advance the link state machine, refresh cache
void gpsPause();   // sleep boundary: drop the link and invalidate the grade anchor

// --- Live readings ---------------------------------------------------------
// All getters are pure reads of a cache refreshed by gpsTick(). TinyGPSPlus
// accessors clear their own `updated` flag as a side effect, so they are
// called exactly once per tick and never from render code.

bool     gpsHasFix();        // valid position, fresher than 5 s
uint8_t  gpsSatellites();
float    gpsHdop();
float    gpsAltitudeM();     // EMA-smoothed
float    gpsSpeedKmh();
float    gpsCourseDeg();
const char* gpsCardinal();   // "N".."NNW", "--" without fix
uint32_t gpsFixAgeMs();

// Road grade in percent, derived from smoothed altitude over a ≥200 m
// baseline. NAN whenever it cannot be computed honestly (no fix, HDOP too
// high, or below the speed gate) — the screen renders "--" rather than a
// number the sensor cannot support.
float    gpsGradePct();

// Why the grade is unavailable. Distinguishing these is the difference
// between "the sensor is broken" and "you are parked" — the GPS debug screen
// shows it so a "--" on the main screen is never a mystery.
enum class GpsGradeGate : uint8_t {
    OK,         // grade is live
    NO_FIX,
    HDOP,       // fix too loose
    SPEED,      // below the speed gate
    BASELINE,   // still travelling toward the 200 m baseline
};
GpsGradeGate gpsGradeGate();

// Metres travelled from the current grade anchor, and the baseline it has to
// reach. Lets the debug screen show progress instead of a dead "--".
float gpsGradeAnchorM();
float gpsGradeBaselineM();
float gpsAltitudeRawM();     // unsmoothed GGA altitude, for comparison

double gpsLat();
double gpsLon();

// --- Link / diagnostics ---------------------------------------------------

GpsLink     gpsLink();
const char* gpsLinkLabel();
uint32_t    gpsBaud();       // rate the link is currently open at

// --- Observed configuration ----------------------------------------------
//
// Result of the last VERIFYING pass, item by item: what we asked the receiver
// for, and what it actually reported. `got == GPS_CFG_NO_REPLY` means the poll
// timed out. This is the honest answer to "did the UBX package apply?" — it
// reflects the receiver, not what the firmware believes it sent.

constexpr int32_t GPS_CFG_NO_REPLY = -1;

struct GpsCfgItem {
    const char* name;    // "DYN", "RATE", "BAUD", "GGA", "GSV", ...
    int32_t     want;
    int32_t     got;
    bool        ok;
};

uint8_t            gpsCfgCount();
const GpsCfgItem&  gpsCfgItem(uint8_t i);
bool               gpsCfgAllOk();   // every item verified this pass

uint32_t gpsCharsProcessed();
uint32_t gpsFailedChecksum();
uint32_t gpsAckCount();
uint32_t gpsNakCount();

// Times the UBX config package has been (re)applied this power cycle. Expected
// to reach 1 on a factory-fresh module and then stay there: if it climbs on
// every boot, either the CFG-CFG save is not sticking or a poll comparison is
// wrong.
uint32_t gpsConfigRuns();
