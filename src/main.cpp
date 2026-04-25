#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "Tft.h"
#include "BootScreen.h"
#include "Buttons.h"
#include "ResetScreen.h"
#include "Telemetry.h"
#include "TripLog.h"
#include "PowerState.h"
#include "SystemScreen.h"
#include "ConsumptionScreen.h"
#include "AutonomyScreen.h"
#include "HistoryScreen.h"
#include "TripSummaryScreen.h"
#include "config.h"

using namespace pobc;

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      (-3 * 3600)   // Brasília UTC-3
#define DAYLIGHT_OFFSET_SEC 0

// --- Screen registry -------------------------------------------------------

struct ScreenDef {
    void     (*draw)();
    ResetSet (*resets)();
};

static const ScreenDef SCREENS[] = {
    { displaySystem,      systemResets      },
    { displayConsumption, consumptionResets },
    { displayAutonomy,    autonomyResets    },
    { displayHistory,     historyResets     },
};
static constexpr uint8_t SCREEN_COUNT  = sizeof(SCREENS) / sizeof(SCREENS[0]);
static constexpr uint8_t HOME_SCREEN   = 1;   // ConsumptionScreen — main / "home"

static uint8_t g_screen = HOME_SCREEN;

// --- Boot messages ---------------------------------------------------------

static void showMessage(const char* msg) {
    targetBackBuffer();
    fillScreenU(COL_BG);
    drawCenteredU(msg, USR_H / 2 - 16, 1, 1, COL_AMBER);
    flipBuffers();
}

static void connectWiFi() {
    Serial.print("Conectando ao WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

static void setupOTA() {
    ArduinoOTA.setHostname("pobc");
    ArduinoOTA
        .onStart([]() { showMessage("Atualizando..."); })
        .onEnd([]()   { showMessage("OK, reiniciando"); })
        .onError([](ota_error_t) { showMessage("Falha OTA"); });
    ArduinoOTA.begin();
    Serial.println("OTA pronto em pobc.local");
}

static void syncTime() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Hora sincronizada.");
}

// --- Input → navigation ----------------------------------------------------

static void handleNavEvent(ButtonEvent ev) {
    switch (ev) {
        case BTN_EV_R_TAP:
            g_screen = (g_screen + SCREEN_COUNT - 1) % SCREEN_COUNT;
            break;
        case BTN_EV_S_TAP:
            g_screen = (g_screen + 1) % SCREEN_COUNT;
            break;
        case BTN_EV_R_HOLD: {
            const ResetSet rs = SCREENS[g_screen].resets();
            resetScreenEnter(rs);
            break;
        }
        case BTN_EV_S_HOLD:
            g_screen = HOME_SCREEN;
            break;
        default:
            break;
    }
}

// Thin amber/green progress bar at the very bottom of the screen so the
// driver gets immediate feedback that a hold is in progress (and how much
// longer they need to keep pressing). R = amber (destructive intent),
// S = green (navigation home).
static void drawHoldOverlay() {
    const uint32_t hr = buttonHoldMs(BTN_R);
    const uint32_t hs = buttonHoldMs(BTN_S);
    if (hr == 0 && hs == 0) return;
    const ButtonId active   = (hr >= hs) ? BTN_R : BTN_S;
    const uint32_t held     = (hr > hs) ? hr : hs;
    const uint32_t threshold = buttonsGetHoldMs(active);
    const uint16_t color    = (active == BTN_R) ? COL_AMBER : COL_GOOD;
    if (threshold == 0) return;
    const int w = (int)((uint64_t)USR_W * held / threshold);
    if (w > 0) fillRectU(0, USR_H - 3, w, 3, color);
}

// --- Arduino entry points --------------------------------------------------

// GPIO 13 — ignition input. Declared here so the deep-sleep entry path
// can configure ext0 without dragging PowerState's internal constants
// across the module boundary.
static constexpr gpio_num_t IGN_PIN = GPIO_NUM_13;

void setup() {
    Serial.begin(115200);
    delay(200);

    const esp_reset_reason_t rr           = esp_reset_reason();
    const bool               wokeFromDeep = (rr == ESP_RST_DEEPSLEEP);
    Serial.printf("\n[P-OBC] boot (reset reason %d, %s)\n",
                  (int)rr, wokeFromDeep ? "deep-sleep wake" : "cold");

    buttonsInit();
    telemetryInit();
    tripLogInit();
    powerInit();   // reads GPIO 14, recovers stash, picks initial state

    tftInit();

    // Skip the splash + WiFi/NTP cost when we just woke from deep sleep:
    // the driver wants the UI immediately. DS3231 keeps the clock alive
    // through deep sleep so missing NTP for one ignition cycle is fine.
    if (powerCurrent() == PowerState::ACTIVE && !wokeFromDeep) {
        displayBoot();
        connectWiFi();
        setupOTA();
        syncTime();
    } else if (powerCurrent() == PowerState::ACTIVE) {
        // Fast path on deep-sleep wake — bring up OTA but skip the
        // blocking WiFi/NTP wait so the dashboard appears within a frame.
        connectWiFi();
        setupOTA();
        syncTime();
    }
    // If powerCurrent() == DEEP_SLEEP_PENDING (cold boot, ignition off),
    // loop()'s very first iteration drops us straight back to deep sleep
    // without lighting up WiFi.
}

// Target frame period. Heaviest screen (Consumption: DSEG7 hero + 12 bars +
// footer) takes ~60-70 ms to render plus the 20 ms vsync-latch wait inside
// flipBuffers, so the loop is padded up to this value. Light screens (Duty)
// wait the rest. Result: every screen iterates at the same rate, animations
// look uniform, and the hold-bar overlay feels consistent across screens.
static constexpr uint32_t FRAME_PERIOD_MS = 100;

// Render and flip a single all-black frame so the panel goes dark before
// the MCU drops into light sleep. LT7680 SDRAM keeps the framebuffer alive
// during sleep so this stays visibly off until we wake.
static void blankScreen() {
    targetBackBuffer();
    fillScreenU(COL_BG);
    flipBuffers();
}

static void enterLightSleep() {
    // Wake on ignition return (GPIO 14 going LOW) or after the remaining
    // grace window has fully elapsed in real time.
    esp_sleep_enable_ext0_wakeup(IGN_PIN, 0);   // 0 = wake on LOW
    esp_sleep_enable_timer_wakeup(250000ULL);   // re-check every 250 ms
                                                 // so the grace timeout
                                                 // and hold-overlay UI
                                                 // resume promptly even
                                                 // without ignition events
    esp_light_sleep_start();
}

static void enterDeepSleep() {
    // Hold the pull-up alive across deep sleep so the line doesn't float.
    rtc_gpio_pullup_en(IGN_PIN);
    rtc_gpio_pulldown_dis(IGN_PIN);
    esp_sleep_enable_ext0_wakeup(IGN_PIN, 0);   // 0 = wake on LOW
    esp_deep_sleep_start();
    // Never returns.
}

void loop() {
    const uint32_t frameStartMs = millis();

    // 1. Power state — must run before anything that reads telemetry,
    //    so a transition into POST_TRIP_SUMMARY / GRACE / DEEP_SLEEP is
    //    visible to the rest of the frame.
    powerTick();

    // Branch by power state. ACTIVE keeps the original behavior; the
    // other states have tailored loop bodies.
    switch (powerCurrent()) {
        case PowerState::ACTIVE: {
            telemetryTick();

            // Contextual hold thresholds:
            //   - Outside the modal, S is "go home" — fast, non-destructive
            //     nav gesture, 1.5 s feels right.
            //   - Inside the modal, S confirms reset[1] — destructive, full
            //     3 s margin. R stays at 3 s everywhere (always destructive
            //     entry).
            buttonsSetHoldMs(BTN_R, BUTTONS_HOLD_DEFAULT_MS);
            buttonsSetHoldMs(BTN_S, resetScreenActive() ? BUTTONS_HOLD_DEFAULT_MS
                                                        : BUTTONS_HOLD_NAV_MS);

            const ButtonEvent ev = buttonsPoll();

            if (resetScreenActive()) {
                resetScreenTick(ev);
            } else {
                handleNavEvent(ev);
            }

            targetBackBuffer();
            if (resetScreenActive()) {
                displayReset();
            } else {
                SCREENS[g_screen].draw();
                drawHoldOverlay();
            }
            flipBuffers();

            ArduinoOTA.handle();
            tftTick();
            break;
        }

        case PowerState::POST_TRIP_SUMMARY: {
            // No telemetryTick — engine is off, integrators must not advance.
            // Any button tap skips the countdown and drops to GRACE early.
            const ButtonEvent ev = buttonsPoll();
            if (ev == BTN_EV_R_TAP || ev == BTN_EV_S_TAP ||
                ev == BTN_EV_R_HOLD || ev == BTN_EV_S_HOLD) {
                powerSkipPostTripSummary();
            }

            targetBackBuffer();
            displayTripSummary();
            flipBuffers();
            tftTick();
            break;
        }

        case PowerState::GRACE: {
            // Pause telemetry's wall-clock anchor so the post-wake tick
            // doesn't see a giant delta and inject phantom kilometers.
            telemetryPause();
            blankScreen();
            enterLightSleep();
            // Resumes here on ignition wake or the periodic 250 ms timer.
            telemetryPause();
            break;
        }

        case PowerState::DEEP_SLEEP_PENDING: {
            blankScreen();
            enterDeepSleep();
            // Never returns — the next wake re-enters setup().
            break;
        }
    }

    // Pacing only applies in interactive states. GRACE handles its own
    // sleep duration; DEEP_SLEEP_PENDING never reaches here.
    if (powerCurrent() == PowerState::ACTIVE ||
        powerCurrent() == PowerState::POST_TRIP_SUMMARY) {
        const uint32_t elapsed = millis() - frameStartMs;
        if (elapsed < FRAME_PERIOD_MS) delay(FRAME_PERIOD_MS - elapsed);
    }
}
