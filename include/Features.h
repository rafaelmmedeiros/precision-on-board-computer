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

} // namespace pobc
