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

// --- Live readings -------------------------------------------------------

float telemetrySpeedKmh();        // current speed
float telemetryKmL();             // instant km/L (smoothed)
bool  telemetryFuelCut();         // DFCO — injectors closed
float telemetryTankL();           // current fuel level in tank
float telemetryTankCapacityL();   // nominal tank capacity (52L on Astra)
float telemetryVoltage();
float telemetryTempInt();
float telemetryTempExt();

// --- Trip accumulators (resettable) --------------------------------------

float    telemetryTripKm();
float    telemetryTripL();
uint32_t telemetryTripSec();

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
