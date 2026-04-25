#pragma once

#include <Arduino.h>

// Persistent trip history. Last N=12 closed trips are kept in NVS
// (Preferences namespace "triplog"), survives reboots, no extra hardware.
// Writes only happen on trip end — wear is a non-issue.
//
// Lifecycle:
//   Boot    → tripLogInit()  loads N records from NVS
//   Trip end (manual encerramento or, later, ignition-off > 1h auto):
//           → tripLogFinishCurrentTrip() snapshots Telemetry into a record,
//             appends to the log, writes NVS, resets Telemetry trip state
//   Apagar  → tripLogClear() wipes both RAM and NVS

constexpr int TRIP_LOG_MAX = 12;

struct TripRecord {
    uint32_t startUnixSec;   // 0 if NTP wasn't synced when trip began
    uint32_t durationSec;    // engine-running time (sim seconds on bench)
    float    km;
    float    liters;
    float    avgKmL;         // = km / liters (precomputed for fast render)
    float    minKmL;         // best-case sample during trip
    float    maxKmL;         // best-case sample (highest km/L seen)
};
static_assert(sizeof(TripRecord) == 28, "TripRecord layout drift — check NVS compatibility");

void tripLogInit();
void tripLogFinishCurrentTrip();   // snapshot + append + reset Telemetry
void tripLogClear();                // wipes log (used by HistoryScreen reset)

int                 tripLogCount();
const TripRecord&   tripLogAt(int i);   // i=0 oldest, count-1 newest
