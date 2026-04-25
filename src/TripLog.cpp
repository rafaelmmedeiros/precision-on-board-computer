#include "TripLog.h"
#include "Telemetry.h"
#include "Features.h"

#include <Preferences.h>
#include <time.h>

using namespace pobc;

static constexpr const char* PREF_NAMESPACE = "triplog";
static constexpr const char* KEY_COUNT      = "count";
static constexpr const char* KEY_DATA       = "data";
static constexpr const char* KEY_STASH      = "stash";

// Serialised in-progress trip for brownout recovery during the grace
// window. Layout is private to TripLog — bumped if fields change.
struct StashRecord {
    uint32_t stashedAtUnix;   // 0 if NTP wasn't synced yet
    uint32_t startUnixSec;
    uint32_t durationSec;
    float    km;
    float    liters;
    float    minKmL;
    float    maxKmL;
    float    tankL;
};
static_assert(sizeof(StashRecord) == 32, "StashRecord layout drift — check NVS compatibility");

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

// --- Stash -----------------------------------------------------------------

void tripLogStashInProgress() {
    StashRecord s;
    s.stashedAtUnix = (uint32_t)time(nullptr);   // 0 if NTP not yet synced
    s.startUnixSec  = telemetryTripStartUnix();
    s.durationSec   = telemetryTripSec();
    s.km            = telemetryTripKm();
    s.liters        = telemetryTripL();
    s.minKmL        = telemetryTripMinKmL();
    s.maxKmL        = telemetryTripMaxKmL();
    s.tankL         = telemetryTankL();

    Preferences p;
    if (!p.begin(PREF_NAMESPACE, false)) return;
    p.putBytes(KEY_STASH, &s, sizeof(s));
    p.end();
}

void tripLogClearStash() {
    Preferences p;
    if (!p.begin(PREF_NAMESPACE, false)) return;
    p.remove(KEY_STASH);
    p.end();
}

bool tripLogHasStash() {
    Preferences p;
    if (!p.begin(PREF_NAMESPACE, true)) return false;
    const bool exists = p.isKey(KEY_STASH);
    p.end();
    return exists;
}

bool tripLogConsumeStashOrFinalize() {
    Preferences p;
    if (!p.begin(PREF_NAMESPACE, true)) return false;
    if (!p.isKey(KEY_STASH)) {
        p.end();
        return false;
    }
    StashRecord s{};
    const size_t got = p.getBytes(KEY_STASH, &s, sizeof(s));
    p.end();
    if (got != sizeof(s)) {
        // Corrupt stash — wipe and bail.
        tripLogClearStash();
        return false;
    }

    const uint32_t now = (uint32_t)time(nullptr);
    const bool clockOk = (s.stashedAtUnix != 0 && now != 0 && now >= s.stashedAtUnix);
    const uint32_t elapsedMs = clockOk
        ? (uint32_t)((uint64_t)(now - s.stashedAtUnix) * 1000UL)
        : UINT32_MAX;   // unknown elapsed → conservative path

    if (clockOk && elapsedMs < GRACE_PERIOD_MS) {
        // Fresh stash — resume the trip in memory.
        telemetryRestoreTrip(s.startUnixSec, s.durationSec, s.km, s.liters,
                             s.minKmL, s.maxKmL, s.tankL);
        tripLogClearStash();
        return true;
    }

    // Stale (or untrustworthy clock) — synthesize a TripRecord from the
    // stash and finalize it directly into the log without touching live
    // Telemetry, which still holds whatever telemetryInit() set up.
    TripRecord r;
    r.startUnixSec = s.startUnixSec;
    r.durationSec  = s.durationSec;
    r.km           = s.km;
    r.liters       = s.liters;
    r.avgKmL       = (r.liters > 0.0001f) ? (r.km / r.liters) : 0.0f;
    r.minKmL       = s.minKmL;
    r.maxKmL       = s.maxKmL;
    if (r.km >= MIN_TRIP_KM) {
        if (g_count < TRIP_LOG_MAX) {
            g_log[g_count++] = r;
        } else {
            for (int i = 0; i < TRIP_LOG_MAX - 1; ++i) g_log[i] = g_log[i + 1];
            g_log[TRIP_LOG_MAX - 1] = r;
        }
        persistToNvs();
    }
    tripLogClearStash();
    return false;
}

// --- Public API (continued) ------------------------------------------------

int tripLogCount() {
    return g_count;
}

const TripRecord& tripLogAt(int i) {
    static const TripRecord zero = {0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    if (i < 0 || i >= g_count) return zero;
    return g_log[i];
}
