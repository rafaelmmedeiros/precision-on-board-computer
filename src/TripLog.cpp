#include "TripLog.h"
#include "Telemetry.h"

#include <Preferences.h>

static constexpr const char* PREF_NAMESPACE = "triplog";
static constexpr const char* KEY_COUNT      = "count";
static constexpr const char* KEY_DATA       = "data";

// Minimum km a trip must cover to be worth saving. Avoids polluting the
// log with accidental encerramentos right after a reset (km < 1 = either
// stuck in DFCO or didn't really go anywhere).
static constexpr float MIN_TRIP_KM = 1.0f;

static TripRecord g_log[TRIP_LOG_MAX];
static int        g_count = 0;

// --- NVS helpers -----------------------------------------------------------

static void persistToNvs() {
    Preferences p;
    if (!p.begin(PREF_NAMESPACE, false)) return;
    p.putInt(KEY_COUNT, g_count);
    p.putBytes(KEY_DATA, g_log, sizeof(TripRecord) * (size_t)g_count);
    p.end();
}

// --- Public API ------------------------------------------------------------

void tripLogInit() {
    Preferences p;
    if (!p.begin(PREF_NAMESPACE, true)) {   // read-only first
        g_count = 0;
        return;
    }
    g_count = p.getInt(KEY_COUNT, 0);
    if (g_count < 0 || g_count > TRIP_LOG_MAX) g_count = 0;   // corrupt → wipe
    if (g_count > 0) {
        const size_t want = sizeof(TripRecord) * (size_t)g_count;
        const size_t got  = p.getBytes(KEY_DATA, g_log, want);
        if (got != want) g_count = 0;       // partial blob → wipe
    }
    p.end();
}

void tripLogFinishCurrentTrip() {
    TripRecord r;
    r.startUnixSec = telemetryTripStartUnix();
    r.durationSec  = telemetryTripSec();
    r.km           = telemetryTripKm();
    r.liters       = telemetryTripL();
    r.avgKmL       = (r.liters > 0.0001f) ? (r.km / r.liters) : 0.0f;
    r.minKmL       = telemetryTripMinKmL();
    r.maxKmL       = telemetryTripMaxKmL();

    if (r.km < MIN_TRIP_KM) {
        // Trivial — reset without saving.
        telemetryResetTrip();
        return;
    }

    if (g_count < TRIP_LOG_MAX) {
        g_log[g_count++] = r;
    } else {
        for (int i = 0; i < TRIP_LOG_MAX - 1; ++i) g_log[i] = g_log[i + 1];
        g_log[TRIP_LOG_MAX - 1] = r;
    }
    persistToNvs();
    telemetryResetTrip();
}

void tripLogClear() {
    g_count = 0;
    Preferences p;
    if (p.begin(PREF_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
}

int tripLogCount() {
    return g_count;
}

const TripRecord& tripLogAt(int i) {
    static const TripRecord zero = {0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    if (i < 0 || i >= g_count) return zero;
    return g_log[i];
}
