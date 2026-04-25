#pragma once

#include <Arduino.h>

// Telemetry — single source of truth for vehicle state. All screens render
// from this layer; nobody writes to it except Telemetry itself.
//
// Phase 1 (bench): values are simulated by a regime state machine
// (cruise / uphill / downhill+DFCO) inside Telemetry.cpp. Tank drain is
// tied to the simulated km/L, then amplified by a bench-only scale factor
// so the gauge animates fast enough to inspect on the desk. When real
// sensors come online (Phase 2-4), the simulators inside Telemetry are
// replaced with sensor readers — no screen touches that boundary.
//
// Tick contract: telemetryTick() must be called once per main-loop
// iteration, before any screen is rendered. Skipping ticks freezes time
// for everyone; calling more than once per loop double-integrates trip /
// tank / history.

void telemetryInit();
void telemetryTick();

// Tells Telemetry that the MCU is about to enter sleep (or has just woken
// from one) and any wall-clock delta accumulated since the last tick must
// be discarded. Without this, the resume tick would see a huge dt and
// inflate trip integrators with phantom kilometers. Safe to call multiple
// times back-to-back.
void telemetryPause();

// --- Live readings -------------------------------------------------------

float telemetrySpeedKmh();        // current speed
float telemetryKmL();             // instant km/L (smoothed)
bool  telemetryFuelCut();         // DFCO — injectors closed
float telemetryTankL();           // current fuel level in tank
float telemetryTankCapacityL();   // nominal tank capacity (52L on Astra)
float telemetryVoltage();
float telemetryTempInt();
float telemetryTempExt();

// True when the headlights are on. Sampled directly off GPIO 35 each
// call (no smoothing — backlight transitions of a few ms are
// imperceptible). External 10k pull-up is mandatory because GPIO 35 is
// input-only with no internal pulls.
bool  telemetryHeadlightOn();

// --- Trip accumulators (resettable) --------------------------------------

float    telemetryTripKm();
float    telemetryTripL();
uint32_t telemetryTripSec();
uint32_t telemetryTripStartUnix();   // epoch sec when trip began (0 if no NTP yet)
float    telemetryTripMinKmL();      // 0 if no samples
float    telemetryTripMaxKmL();      // 0 if no samples

// --- 5-min km/L history --------------------------------------------------
// Slot 0 is the live (instant) value; slots 1..count are locked 5-min
// averages, with slot 1 being the most recent and shifting right as new
// slots close.

constexpr int TELEMETRY_HIST_N = 12;

int   telemetryHistCount();           // number of locked slots, 0..N-1
float telemetryHistAt(int slot);      // 0=live, 1..count=locked

// Mean and standard deviation over live + locked slots, with an honesty
// floor on stddev so early-trip range numbers don't claim false precision.
void  telemetryGetKmLStats(float& mean, float& stddev);

// --- Resets --------------------------------------------------------------

void telemetryResetTrip();

// --- Stash restore -------------------------------------------------------
// Used by TripLog when an in-progress trip is recovered from NVS after a
// reset that happened during the grace window (rare — light sleep keeps
// RAM, so this only matters for brownouts). History bars and the in-flight
// 5-min period accumulator are NOT restored: those are cosmetic and a
// fresh start is acceptable. The trip totals (the user-visible numbers)
// are preserved.

void telemetryRestoreTrip(uint32_t startUnix,
                          uint32_t durationSec,
                          float    km,
                          float    liters,
                          float    minKmL,
                          float    maxKmL,
                          float    tankL);
