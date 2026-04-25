#pragma once

// Compile-time feature configuration. Each flag below decides whether a
// given vehicle signal comes from a real sensor or from the bench
// simulator inside Telemetry. Flip a flag to `true` once the hardware is
// wired up and its reader is implemented.
//
// Default everywhere is `false` — pure simulation, suitable for desk
// development. Telemetry uses `if constexpr` on these flags so unused
// branches are completely eliminated by the compiler; there is no runtime
// cost and no dead-code in the firmware image.

namespace pobc {

// === Sensor flags =========================================================

constexpr bool USE_REAL_INJECTOR     = false; // GPIO 34 — pulse-width interrupt
constexpr bool USE_REAL_VSS          = false; // GPIO 36 — vehicle speed signal
constexpr bool USE_REAL_VOLTAGE      = false; // GPIO 32 — battery + ignition ADC
constexpr bool USE_REAL_FUEL_LEVEL   = false; // GPIO 33 — boia ADC
constexpr bool USE_REAL_TEMP_SENSORS = false; // GPIO 4  — DS18B20 OneWire
constexpr bool USE_REAL_GPS          = false; // UART2
constexpr bool USE_REAL_IGNITION     = false; // GPIO 13 — ignition digital input
                                              //   Bench and production share the
                                              //   same active-low topology (switch
                                              //   or opto pulls pin to GND), so the
                                              //   PowerState reader is identical
                                              //   for both. Flag exists for
                                              //   coherence with the other sensors
                                              //   and as a hook for future opto
                                              //   polarity changes.

// True when at least one signal is still simulated. Used by Telemetry to
// decide whether the bench fast-forward applies.
constexpr bool ANY_SIMULATED =
       !USE_REAL_INJECTOR
    || !USE_REAL_VSS
    || !USE_REAL_VOLTAGE
    || !USE_REAL_FUEL_LEVEL
    || !USE_REAL_TEMP_SENSORS;

// === Bench tuning =========================================================

// Multiplier applied to the integration step (Δt) inside Telemetry's
// simulator. With 1.0 the sim runs at wall-clock rate — a 52L tank empties
// in ~8 hours at cruise, which is invisible on the desk. Higher values
// fast-forward simulated vehicle time so gauges animate at a useful pace.
//
// At BENCH_TIME_SCALE = 60.0:
//   - 1 real second equals 1 simulated minute
//   - 5-min km/L history slot closes every 5 real seconds
//   - Tank cycles full → empty → refuel in ~8 real minutes
//   - Trip footer (HH:MM) advances ~60x faster than wall clock
//
// Coherence: every integrator (trip km, trip L, tank, history, trip time)
// shares this scale so the numbers across screens stay self-consistent.
//
// Real-sensor branches ignore this value — they read wall-clock signals.
constexpr float BENCH_TIME_SCALE = 60.0f;

// === Power state machine =================================================

// How long the trip-summary screen stays visible after the key turns off,
// before the MCU drops the display and enters light sleep (GRACE).
constexpr uint32_t POST_TRIP_SUMMARY_MS = 60UL * 1000UL;            // 60 s

// Bench shortcut for POST_TRIP_SUMMARY_MS — same pattern as
// BENCH_GRACE_MS_OVERRIDE. 0 = use POST_TRIP_SUMMARY_MS as-is. Ignored
// when USE_REAL_IGNITION is true so production always uses the full 60s.
constexpr uint32_t BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE = 10UL * 1000UL; // 10 s on bench

// How long the MCU keeps the trip alive in light sleep waiting for the
// driver to come back. If ignition returns before this elapses, the same
// trip resumes; if not, the trip is finalized into the log and the MCU
// drops to deep sleep until the next ignition wake.
constexpr uint32_t GRACE_PERIOD_MS = 60UL * 60UL * 1000UL;          // 1 h

// Bench shortcut for the grace period. 0 = use GRACE_PERIOD_MS as-is.
// Set to e.g. 30000 to watch the full POST_TRIP_SUMMARY → GRACE → deep
// sleep cycle in seconds instead of waiting an hour. Not applied when
// USE_REAL_IGNITION is true (production must use the real timeout).
constexpr uint32_t BENCH_GRACE_MS_OVERRIDE = 30UL * 1000UL;         // 30 s on bench

} // namespace pobc
