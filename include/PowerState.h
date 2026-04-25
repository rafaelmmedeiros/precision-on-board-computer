#pragma once

#include <Arduino.h>

// Always-on power state machine.
//
// Hardware contract: the ESP32 is wired to a permanent 12V battery feed,
// so it stays energised whether the car is running or parked. The driver's
// ignition key is sensed as a digital active-low signal on GPIO 13
// (INPUT_PULLUP + switch/opto to GND, same topology as the R/S buttons).
//
// State flow (see plan):
//   ACTIVE              — ignition on, full UI, sensors, telemetryTick.
//   POST_TRIP_SUMMARY   — ignition just went off, summary screen visible
//                         for POST_TRIP_SUMMARY_MS or until any tap.
//   GRACE               — display dark, MCU in light sleep; trip kept in
//                         RAM. Wakes on ignition return (resume same trip)
//                         or on grace-period timeout (finalize trip,
//                         drop into DEEP_SLEEP_PENDING).
//   DEEP_SLEEP_PENDING  — pseudo-state: main loop will call
//                         esp_deep_sleep_start() on its next iteration.
//
// powerTick() handles all transitions. main.cpp consults powerCurrent()
// to decide whether to render screens, enter sleep, etc.

namespace pobc {

enum class PowerState : uint8_t {
    ACTIVE,
    POST_TRIP_SUMMARY,
    GRACE,
    DEEP_SLEEP_PENDING,
};

// Call once early in setup(), AFTER telemetryInit() and tripLogInit() so
// the brownout-recovery path can restore an in-progress trip from NVS
// before any other module looks at telemetry. Configures GPIO 13, picks
// initial state from current ignition reading + reset reason.
void powerInit();

// Call once per loop iteration, before telemetryTick() or rendering.
// Reads ignition (debounced 50 ms) and runs the state transitions.
void powerTick();

PowerState powerCurrent();

// Wall-clock ms spent in the current state. Used by the trip-summary
// countdown and by the grace timeout check.
uint32_t powerStateElapsedMs();

// Live debounced ignition reading. True when GPIO 13 is LOW (key on).
bool ignitionOn();

// Driver tapped a button on the post-trip summary screen — skip the
// remaining countdown and transition to GRACE on the next powerTick().
void powerSkipPostTripSummary();

// Effective post-trip-summary timeout: collapses the bench override into
// POST_TRIP_SUMMARY_MS when ignition is real, otherwise honours the
// override. Used by both PowerState and TripSummaryScreen so the
// countdown shown on screen always matches the actual transition.
uint32_t powerPostTripSummaryMs();

} // namespace pobc
