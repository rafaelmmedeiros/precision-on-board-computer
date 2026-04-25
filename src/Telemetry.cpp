#include "Telemetry.h"
#include "Features.h"

#include <Arduino.h>
#include <math.h>
#include <time.h>

using namespace pobc;

// === Vehicle constants ====================================================

static constexpr float SIM_SPEED_KMH    = 80.0f;     // constant cruise sim
static constexpr float SCALE_MAX_KMPL   = 20.0f;     // top of UI chart band
static constexpr float TANK_CAPACITY_L  = 52.0f;     // Astra 2.0 8V Flex
static constexpr float TANK_INITIAL_L   = 40.0f;     // bench: start mid-range
static constexpr float TANK_REFUEL_AT_L = 1.0f;      // bench: auto-refuel

static constexpr uint32_t HIST_PERIOD_MS = 5UL * 60UL * 1000UL;   // sim ms

// === Regime state machine =================================================

enum class Regime : uint8_t { Cruise, Uphill, Downhill };

static Regime    g_regime       = Regime::Cruise;
static uint32_t  g_regimeEndMs  = 0;     // wall-clock — regime cadence is
                                          // a property of the human watching
                                          // the bench, not of vehicle physics
static float     g_regimeTarget = 12.0f;
static float     g_smoothed     = 12.0f;

static void pickNextRegime() {
    const uint32_t now = millis();
    const long r = random(100);
    if (r < 70) {
        g_regime       = Regime::Cruise;
        g_regimeTarget = 11.5f + random(30) / 10.0f;     // 11.5 .. 14.5
        g_regimeEndMs  = now + 30000UL + random(40000);   // 30 .. 70 s
    } else if (r < 87) {
        g_regime       = Regime::Uphill;
        g_regimeTarget = 6.0f + random(25) / 10.0f;      // 6.0 .. 8.5
        g_regimeEndMs  = now + 12000UL + random(15000);   // 12 .. 27 s
    } else {
        g_regime       = Regime::Downhill;
        g_regimeTarget = SCALE_MAX_KMPL;                  // pegged at 20
        g_regimeEndMs  = now + 8000UL + random(15000);    // 8 .. 23 s
    }
}

static float simKmL() {
    if (millis() >= g_regimeEndMs) pickNextRegime();
    g_smoothed += (g_regimeTarget - g_smoothed) * 0.06f;
    if (g_regime == Regime::Cruise) {
        g_smoothed += 0.08f * sinf(millis() / 700.0f);
    }
    return g_smoothed;
}

// === Live state ===========================================================

static float    g_kmL    = 12.0f;
static bool     g_dfco   = false;

// === Tank =================================================================

static float    g_tankL  = TANK_INITIAL_L;

// === Trip — simulated vehicle time, not wall clock ========================
//
// On the bench with BENCH_TIME_SCALE > 1, "trip seconds" are simulated
// vehicle seconds. After 1 real minute at scale=60, the trip says 1:00
// (one simulated hour) and shows ~80 km traveled — the numbers stay
// self-consistent. With real sensors, scale collapses to 1.0 and these
// become wall-clock seconds again.

static double   g_tripSimSec = 0.0;
static double   g_tripKm     = 0.0;
static double   g_tripL      = 0.0;
static uint32_t g_lastRealMs = 0;
static uint32_t g_tripStartUnix = 0;     // epoch sec, 0 until NTP synced
static float    g_tripMinKmL = 0.0f;     // 0 until first sample
static float    g_tripMaxKmL = 0.0f;

// === History =============================================================

static float    g_history[TELEMETRY_HIST_N] = {0};
static int      g_populated  = 0;
static uint32_t g_periodSimMs = 0;
static double   g_periodSum  = 0.0;
static int      g_periodN    = 0;

// === Voltage / temps =====================================================

static float    g_voltage = 13.5f;
static float    g_tempInt = 22.5f;
static float    g_tempExt = 22.5f;

// === Helpers ==============================================================

static float effectiveScale() {
    // Apply the bench fast-forward only while ANY signal is still
    // simulated. Once every flag flips to real, everything reads
    // wall-clock and the scale is forced to 1.0.
    return ANY_SIMULATED ? BENCH_TIME_SCALE : 1.0f;
}

// === Lifecycle ============================================================

void telemetryInit() {
    const uint32_t now = millis();
    g_lastRealMs    = now;
    g_regimeEndMs   = now;   // forces immediate regime pick
    g_tripSimSec    = 0.0;
    g_tripKm        = 0.0;
    g_tripL         = 0.0;
    g_periodSimMs   = 0;
    g_tripStartUnix = (uint32_t)time(nullptr);   // 0 if NTP not yet synced
    g_tripMinKmL    = 0.0f;
    g_tripMaxKmL    = 0.0f;
}

void telemetryTick() {
    const uint32_t now       = millis();
    const float    realDtSec = (now - g_lastRealMs) / 1000.0f;
    g_lastRealMs = now;

    const float scale = effectiveScale();
    const float dtSec = realDtSec * scale;     // vehicle-time delta

    // 1. km/L source -------------------------------------------------------
    if constexpr (USE_REAL_INJECTOR) {
        // TODO Phase 2: derive g_kmL and g_dfco from the GPIO 34 interrupt
        // pulse-width handler.
    } else {
        g_kmL  = simKmL();
        g_dfco = (g_regime == Regime::Downhill);
    }

    // 2. Trip integrators -------------------------------------------------
    // dKm = v · Δt; dL = dKm / km_L (zero during DFCO — no fuel injected).
    // tank decrements by EXACTLY the same dL — boia and bicos can't tell
    // contradictory stories. When the real boia goes online, its reading
    // overrides this whole branch.
    const float dKm = SIM_SPEED_KMH * dtSec / 3600.0f;
    g_tripSimSec += dtSec;
    g_tripKm     += dKm;
    float dL = 0.0f;
    if (!g_dfco && g_kmL > 0.1f) {
        dL = dKm / g_kmL;
        // Track min/max km/L across the trip (only "real driving" samples
        // — DFCO inflates instant km/L because no fuel is being burnt;
        // including those would make the maxKmL field meaningless for
        // the trip log).
        if (g_tripMaxKmL == 0.0f || g_kmL > g_tripMaxKmL) g_tripMaxKmL = g_kmL;
        if (g_tripMinKmL == 0.0f || g_kmL < g_tripMinKmL) g_tripMinKmL = g_kmL;
        // Stamp trip start once NTP delivers a real epoch — handles the
        // common case where boot precedes WiFi+NTP sync by a few seconds.
        if (g_tripStartUnix == 0) g_tripStartUnix = (uint32_t)time(nullptr);
    }
    g_tripL += dL;

    if constexpr (USE_REAL_FUEL_LEVEL) {
        // TODO Phase 1 final: g_tankL = readBoiaAdc() with calibration.
    } else {
        g_tankL -= dL;
        if (g_tankL < TANK_REFUEL_AT_L) g_tankL = TANK_CAPACITY_L;
    }

    // 3. History — slot 0 = live, slots 1..N-1 = locked 5-min averages.
    //    Period accumulator advances in vehicle time so the bench cycles
    //    through 12 slots in roughly one real minute at scale=60.
    g_history[0]  = g_kmL;
    g_periodSum  += g_kmL;
    g_periodN    += 1;
    g_periodSimMs += (uint32_t)(dtSec * 1000.0f);
    if (g_periodSimMs >= HIST_PERIOD_MS && g_periodN > 0) {
        const float avg = (float)(g_periodSum / (double)g_periodN);
        for (int i = TELEMETRY_HIST_N - 1; i >= 2; --i) g_history[i] = g_history[i - 1];
        g_history[1] = avg;
        if (g_populated < TELEMETRY_HIST_N - 1) g_populated++;
        g_periodSum  = 0.0;
        g_periodN    = 0;
        g_periodSimMs = 0;
    }

    // 4. Voltage --------------------------------------------------------
    if constexpr (USE_REAL_VOLTAGE) {
        // TODO Phase 1 final: g_voltage = readVoltageDivider();
    } else {
        const float t = now / 1000.0f;
        g_voltage = 13.25f + 2.25f * sinf(t * 2.0f * (float)M_PI / 30.0f);
    }

    // 5. Temperatures --------------------------------------------------
    if constexpr (USE_REAL_TEMP_SENSORS) {
        // TODO Phase 1 final: read DS18B20 by ROM ID for INT and EXT.
    } else {
        const float t = now / 1000.0f;
        g_tempInt = 22.5f + 17.5f * sinf(t * 2.0f * (float)M_PI / 24.0f + 1.2f);
        g_tempExt = 20.0f + 25.0f * sinf(t * 2.0f * (float)M_PI / 36.0f + 2.4f);
    }
}

// === Getters ==============================================================

float telemetrySpeedKmh()      { return SIM_SPEED_KMH; }
float telemetryKmL()           { return g_kmL; }
bool  telemetryFuelCut()       { return g_dfco; }
float telemetryTankL()         { return g_tankL; }
float telemetryTankCapacityL() { return TANK_CAPACITY_L; }
float telemetryVoltage()       { return g_voltage; }
float telemetryTempInt()       { return g_tempInt; }
float telemetryTempExt()       { return g_tempExt; }

float    telemetryTripKm()        { return (float)g_tripKm; }
float    telemetryTripL()         { return (float)g_tripL; }
uint32_t telemetryTripSec()       { return (uint32_t)g_tripSimSec; }
uint32_t telemetryTripStartUnix() { return g_tripStartUnix; }
float    telemetryTripMinKmL()    { return g_tripMinKmL; }
float    telemetryTripMaxKmL()    { return g_tripMaxKmL; }

int   telemetryHistCount()   { return g_populated; }
float telemetryHistAt(int i) {
    if (i < 0 || i >= TELEMETRY_HIST_N) return 0.0f;
    return g_history[i];
}

void telemetryGetKmLStats(float& mean, float& stddev) {
    const int n = g_populated + 1;
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += g_history[i];
    mean = (n > 0) ? (sum / n) : 12.0f;

    if (n < 2) {
        stddev = mean * 0.15f;
        return;
    }
    float vsum = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float d = g_history[i] - mean;
        vsum += d * d;
    }
    stddev = sqrtf(vsum / n);

    const float floor = mean * 0.05f;
    if (stddev < floor) stddev = floor;
}

void telemetryResetTrip() {
    g_tripSimSec    = 0.0;
    g_tripKm        = 0.0;
    g_tripL         = 0.0;
    g_tripStartUnix = (uint32_t)time(nullptr);
    g_tripMinKmL    = 0.0f;
    g_tripMaxKmL    = 0.0f;
    for (int i = 0; i < TELEMETRY_HIST_N; ++i) g_history[i] = 0.0f;
    g_populated   = 0;
    g_periodSimMs = 0;
    g_periodSum   = 0.0;
    g_periodN     = 0;
}
