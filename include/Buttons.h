#pragma once

#include <Arduino.h>

enum ButtonId : uint8_t { BTN_R = 0, BTN_S = 1 };

enum ButtonEvent : uint8_t {
    BTN_EV_NONE = 0,
    BTN_EV_R_TAP,
    BTN_EV_S_TAP,
    BTN_EV_R_HOLD,
    BTN_EV_S_HOLD,
};

// Default thresholds — set per-button via buttonsSetHoldMs() if context
// requires a different feel (e.g. shorter for non-destructive nav, longer
// for destructive confirms inside a modal).
constexpr uint32_t BUTTONS_HOLD_DEFAULT_MS = 3000;
constexpr uint32_t BUTTONS_HOLD_NAV_MS     = 1500;

void buttonsInit();

// Poll once per loop. Returns at most one queued event per call.
// Hold events fire when the per-button threshold is crossed (button still
// pressed); tap events fire on release if no hold was emitted.
ButtonEvent buttonsPoll();

// Live hold duration (ms, saturated at the button's current threshold).
// Returns 0 when the button is not currently in a "growing toward HOLD"
// state — i.e. after a HOLD event fires, this resets to 0 even though the
// button may still be physically down. Useful for progress-bar UI.
uint32_t buttonHoldMs(ButtonId id);

// Override the hold threshold for a button. Idempotent: setting the same
// value mid-press does not interrupt or reset the press tracking.
void     buttonsSetHoldMs(ButtonId id, uint32_t ms);
uint32_t buttonsGetHoldMs(ButtonId id);
