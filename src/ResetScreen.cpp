#include "ResetScreen.h"
#include "Tft.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

enum class State : uint8_t { Idle, Showing, Toast };

static State        g_state = State::Idle;
static ResetSet     g_opts  = { 0, { { nullptr, nullptr }, { nullptr, nullptr } } };
static uint32_t     g_enteredMs = 0;
static uint32_t     g_toastStartMs = 0;
static const char*  g_toastLabel = nullptr;

static constexpr uint32_t EMPTY_DISMISS_MS = 2000;
static constexpr uint32_t TOAST_MS         = 1500;

void resetScreenEnter(const ResetSet& opts) {
    g_opts        = opts;
    g_state       = State::Showing;
    g_enteredMs   = millis();
    g_toastLabel  = nullptr;
}

bool resetScreenActive() {
    return g_state != State::Idle;
}

static void confirm(uint8_t idx) {
    if (idx >= g_opts.count) return;
    if (g_opts.opts[idx].fn) g_opts.opts[idx].fn();
    g_toastLabel   = g_opts.opts[idx].label;
    g_toastStartMs = millis();
    g_state        = State::Toast;
}

void resetScreenTick(ButtonEvent ev) {
    const uint32_t now = millis();

    switch (g_state) {
        case State::Idle:
            return;

        case State::Showing:
            if (g_opts.count == 0) {
                if (now - g_enteredMs >= EMPTY_DISMISS_MS) g_state = State::Idle;
                if (ev == BTN_EV_R_TAP || ev == BTN_EV_S_TAP) g_state = State::Idle;
                return;
            }
            if (ev == BTN_EV_R_TAP || ev == BTN_EV_S_TAP) {
                g_state = State::Idle;
            } else if (ev == BTN_EV_R_HOLD && g_opts.count >= 1) {
                confirm(0);
            } else if (ev == BTN_EV_S_HOLD && g_opts.count >= 2) {
                confirm(1);
            }
            return;

        case State::Toast:
            if (now - g_toastStartMs >= TOAST_MS) g_state = State::Idle;
            return;
    }
}

static void drawHoldBar(int ux, int uy, int uw, int uh,
                        uint32_t heldMs, uint32_t thresholdMs, uint16_t color) {
    fillRectU(ux, uy, uw, uh, COL_BG);
    fillRectU(ux,         uy,         uw, 1,  color);
    fillRectU(ux,         uy + uh - 1, uw, 1, color);
    fillRectU(ux,         uy,         1,  uh, color);
    fillRectU(ux + uw - 1, uy,         1,  uh, color);
    if (heldMs == 0 || thresholdMs == 0) return;
    const int innerW = uw - 4;
    const int filled = (int)((uint64_t)innerW * heldMs / thresholdMs);
    if (filled > 0) fillRectU(ux + 2, uy + 2, filled, uh - 4, color);
}

static void drawOptionRow(int yLabel, int yBar, char tag, const char* label,
                          uint32_t heldMs, uint32_t thresholdMs) {
    char line[40];
    snprintf(line, sizeof(line), "[%c] %s", tag, label);
    drawCenteredU(line, yLabel, 1, 3, COL_AMBER);
    drawHoldBar(USR_W / 2 - 200, yBar, 400, 14, heldMs, thresholdMs, COL_AMBER);
}

void displayReset() {
    fillScreenU(COL_BG);
    drawCenteredU("RESETS", 18, 1, 2, COL_AMBER);

    if (g_state == State::Toast) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%s RESETADO", g_toastLabel ? g_toastLabel : "");
        drawCenteredU(buf, USR_H / 2 - 24, 1, 3, COL_GOOD);
        return;
    }

    if (g_opts.count == 0) {
        drawCenteredU("Sem resets nesta tela", USR_H / 2 - 16, 1, 2, COL_AMBER);
        return;
    }

    if (g_opts.count == 1) {
        drawOptionRow(120, 200, 'R', g_opts.opts[0].label,
                      buttonHoldMs(BTN_R), buttonsGetHoldMs(BTN_R));
        return;
    }

    drawOptionRow( 80, 144, 'R', g_opts.opts[0].label,
                  buttonHoldMs(BTN_R), buttonsGetHoldMs(BTN_R));
    drawOptionRow(190, 254, 'S', g_opts.opts[1].label,
                  buttonHoldMs(BTN_S), buttonsGetHoldMs(BTN_S));
}
