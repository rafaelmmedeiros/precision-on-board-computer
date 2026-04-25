#include "PowerState.h"
#include "Features.h"
#include "Telemetry.h"
#include "TripLog.h"

#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/rtc_io.h>

namespace pobc {

static constexpr gpio_num_t IGN_PIN      = GPIO_NUM_13;
static constexpr uint32_t   DEBOUNCE_MS  = 50;

static PowerState g_state          = PowerState::ACTIVE;
static uint32_t   g_stateEnteredMs = 0;
static bool       g_skipSummary    = false;

// Debounce — track the last raw read and only commit to the stable value
// after DEBOUNCE_MS of consistency. Active-low: stable LOW = ignition on.
static int      g_lastRaw         = HIGH;
static uint32_t g_lastChangeMs    = 0;
static bool     g_ignitionStable  = false;

static void readIgnitionRaw() {
    const int      now = digitalRead(IGN_PIN);
    const uint32_t t   = millis();
    if (now != g_lastRaw) {
        g_lastRaw      = now;
        g_lastChangeMs = t;
        return;
    }
    if (t - g_lastChangeMs >= DEBOUNCE_MS) {
        g_ignitionStable = (now == LOW);
    }
}

static void enterState(PowerState s) {
    g_state          = s;
    g_stateEnteredMs = millis();
}

// Bench shortcut so the operator doesn't have to wait an hour to watch
// the deep-sleep transition. In production this collapses to the real
// grace period.
static uint32_t graceTimeoutMs() {
    if constexpr (USE_REAL_IGNITION) {
        return GRACE_PERIOD_MS;
    } else {
        return BENCH_GRACE_MS_OVERRIDE > 0 ? BENCH_GRACE_MS_OVERRIDE
                                           : GRACE_PERIOD_MS;
    }
}

uint32_t powerPostTripSummaryMs() {
    if constexpr (USE_REAL_IGNITION) {
        return POST_TRIP_SUMMARY_MS;
    } else {
        return BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE > 0
            ? BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE
            : POST_TRIP_SUMMARY_MS;
    }
}

void powerInit() {
    pinMode(IGN_PIN, INPUT_PULLUP);
    delay(5);   // let the pull-up pull the line up before the first read

    g_lastRaw        = digitalRead(IGN_PIN);
    g_lastChangeMs   = millis();
    g_ignitionStable = (g_lastRaw == LOW);

    const esp_reset_reason_t rr = esp_reset_reason();
    const bool wokeFromDeep     = (rr == ESP_RST_DEEPSLEEP);

    if (!wokeFromDeep) {
        // Cold boot. Recover any in-flight trip stashed before the last
        // shutdown. If the stash is fresh (< grace), telemetry gets the
        // trip restored; if it's stale, the trip is finalised into the
        // log here so it shows up on HistoryScreen.
        tripLogConsumeStashOrFinalize();
    }

    if (g_ignitionStable) {
        enterState(PowerState::ACTIVE);
    } else {
        enterState(PowerState::DEEP_SLEEP_PENDING);
    }
}

void powerTick() {
    readIgnitionRaw();

    switch (g_state) {
        case PowerState::ACTIVE:
            if (!g_ignitionStable) {
                enterState(PowerState::POST_TRIP_SUMMARY);
            }
            break;

        case PowerState::POST_TRIP_SUMMARY:
            if (g_ignitionStable) {
                // Driver came right back — resume the same trip without
                // bouncing through GRACE.
                g_skipSummary = false;
                enterState(PowerState::ACTIVE);
            } else if (g_skipSummary || powerStateElapsedMs() >= powerPostTripSummaryMs()) {
                g_skipSummary = false;
                // Brownout safety net: persist the in-flight trip so a
                // power blip during GRACE doesn't lose it.
                tripLogStashInProgress();
                enterState(PowerState::GRACE);
            }
            break;

        case PowerState::GRACE:
            if (g_ignitionStable) {
                tripLogClearStash();
                enterState(PowerState::ACTIVE);
            } else if (powerStateElapsedMs() >= graceTimeoutMs()) {
                tripLogClearStash();
                tripLogFinishCurrentTrip();
                enterState(PowerState::DEEP_SLEEP_PENDING);
            }
            // Else: main loop will enter light sleep on this iteration.
            break;

        case PowerState::DEEP_SLEEP_PENDING:
            // Main loop calls esp_deep_sleep_start() — never returns.
            break;
    }
}

PowerState powerCurrent()         { return g_state; }
uint32_t   powerStateElapsedMs()  { return millis() - g_stateEnteredMs; }
bool       ignitionOn()           { return g_ignitionStable; }
void       powerSkipPostTripSummary() { g_skipSummary = true; }

} // namespace pobc
