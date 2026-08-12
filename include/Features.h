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

// === Debug screens ========================================================
//
// Declared as BOTH a macro and a constexpr, on purpose. The macro lets
// main.cpp drop the debug entries out of the SCREENS[] initializer with
// `#if`, which is the only way they truly leave the binary — a constexpr
// alone cannot, because the array would still reference the draw functions
// and the linker would keep them. Measured: flipping this to 0 removes
// ~2.1 KB of flash. The constexpr is what the rest of the code branches on,
// so there is still a single source of truth.
//
// 1 = debug screens compiled in, hidden behind the unlock gesture (both
//     buttons held together ~1.2 s) so they never clutter normal navigation.
// 0 = they do not exist at all; the nav ring is production-only.
#define POBC_DEBUG_MODE 1

namespace pobc {

constexpr bool DEBUG_MODE = (POBC_DEBUG_MODE != 0);

// How long both buttons must be held together to reveal / hide the debug
// screens. Must stay below BUTTONS_HOLD_NAV_MS (1500) so the combo is
// recognised before S's own "go home" hold fires.
constexpr uint32_t DEBUG_UNLOCK_MS = 1200;

// === Sensor flags =========================================================

constexpr bool USE_REAL_INJECTOR     = false; // GPIO 34 — pulse-width interrupt
constexpr bool USE_REAL_VSS          = false; // GPIO 36 — vehicle speed signal
constexpr bool USE_REAL_VOLTAGE      = false; // GPIO 32 — battery + ignition ADC
constexpr bool USE_REAL_FUEL_LEVEL   = false; // GPIO 33 — boia ADC
constexpr bool USE_REAL_TEMP_SENSORS = false; // DS18B20 OneWire — pino a alocar
                                              //   (GPIO 4 está em uso pelo painel
                                              //   ST7701S no boot; reuso pós-init
                                              //   é viável, ver CLAUDE.md §4.2)
constexpr bool USE_REAL_GPS          = true;  // GPS NEO-8M — Serial1, RX=GPIO 39
                                              //   (input-only), TX=GPIO 15. A
                                              //   pinagem default de UART2
                                              //   (16/17) foi tomada pelo RESET
                                              //   do LT7680 e pelo CS do painel
                                              //   ST7701S, ver §4.2. Com `false`
                                              //   o GpsModule simula fix, alti-
                                              //   tude, heading e grade pra que
                                              //   o layout da GpsScreen possa
                                              //   ser revisado sem antena.
constexpr bool USE_REAL_IGNITION     = false; // GPIO 13 — ignition digital input
                                              //   Bench and production share the
                                              //   same active-low topology (switch
                                              //   or opto pulls pin to GND), so the
                                              //   PowerState reader is identical
                                              //   for both. Flag exists for
                                              //   coherence with the other sensors
                                              //   and as a hook for future opto
                                              //   polarity changes.
constexpr bool USE_REAL_FAROL        = true;  // GPIO 35 — headlight digital input
                                              //   Same topology as ignition: switch
                                              //   or PC817 pulls pin to GND. GPIO
                                              //   35 is input-only with NO internal
                                              //   pull, so an EXTERNAL 10k pull-up
                                              //   to 3.3V is mandatory (already
                                              //   needed by the production opto
                                              //   collector — single resistor
                                              //   covers both wiring scenarios).
                                              //   Flag is `true` because there is
                                              //   no simulated farol — the bench
                                              //   uses a real toggle switch.

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

// === Backlight =============================================================
//
// Driven by PWM on GPIO 2 (LEDC). On the bench this also blinks the
// onboard LED, which is convenient debug feedback. The pin is meant to
// feed the LT7680 BL_CONTROL input (datasheet pin 9) once the 3.3V
// hard-tie is cut — until then, the PWM signal is generated and visible
// only on the onboard LED.
//
// Levels are 0..255 (LEDC 8-bit). DAY is full brightness, NIGHT mirrors
// the factory dashboard "headlights on" dim, OFF kills the backlight
// entirely (used in GRACE / DEEP_SLEEP).

// Bench levels — kept conservative so the operator's eyes don't burn
// during desk work. Production may want DAY = 255 (full brightness) once
// the unit is in the car under sunlight.
constexpr uint8_t BACKLIGHT_LEVEL_NIGHT     = 20; // 1 to 255
constexpr uint8_t BACKLIGHT_LEVEL_DAY       = 85; // 1 to 255
constexpr uint8_t BACKLIGHT_LEVEL_OFF       = 0;

// Set to true if the breakout drives the BL_CONTROL signal inverted (high
// duty dims, low duty brightens). The current breakout reads "active-high
// + dimmable via duty" — duty up, BL up — so the flag is false. Public
// API in Tft.h (`tftBacklight(0..255)`, 0 dark / 255 bright) is unchanged
// regardless: tftBacklight() flips internally before writing the LEDC
// when this flag is true.
constexpr bool    BACKLIGHT_ACTIVE_LOW  = false;

} // namespace pobc
