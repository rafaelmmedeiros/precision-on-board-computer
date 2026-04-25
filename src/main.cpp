#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "Tft.h"
#include "BootScreen.h"
#include "Buttons.h"
#include "ResetScreen.h"
#include "SystemScreen.h"
#include "ConsumptionScreen.h"
#include "config.h"

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

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[P-OBC] boot");

    buttonsInit();
    tftInit();
    displayBoot();
    connectWiFi();
    setupOTA();
    syncTime();
}

// Target frame period. Heaviest screen (Consumption: DSEG7 hero + 12 bars +
// footer) takes ~60-70 ms to render plus the 20 ms vsync-latch wait inside
// flipBuffers, so the loop is padded up to this value. Light screens (Duty)
// wait the rest. Result: every screen iterates at the same rate, animations
// look uniform, and the hold-bar overlay feels consistent across screens.
static constexpr uint32_t FRAME_PERIOD_MS = 100;

void loop() {
    const uint32_t frameStartMs = millis();

    // Contextual hold thresholds:
    //   - Outside the modal, S is "go home" — a fast, non-destructive nav
    //     gesture, so 1.5 s feels right (3 s was tedious for navigation).
    //   - Inside the modal, S confirms reset[1] — destructive, deserves the
    //     full 3 s margin to avoid accidents. R stays at 3 s everywhere
    //     since it's always the entry to a destructive flow.
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

    const uint32_t elapsed = millis() - frameStartMs;
    if (elapsed < FRAME_PERIOD_MS) delay(FRAME_PERIOD_MS - elapsed);
}
