#include "Buttons.h"

static constexpr uint8_t  PIN_R       = 25;
static constexpr uint8_t  PIN_S       = 26;
static constexpr uint32_t DEBOUNCE_MS = 25;

struct BtnState {
    uint8_t  pin;
    bool     pressed;       // debounced state
    uint32_t edgeMs;        // ms of last accepted state change
    uint32_t pressMs;       // ms when debounced press began
    uint32_t holdMs;        // current hold threshold for this button
    bool     holdEmitted;   // hold event already fired for this press
};

static BtnState g_btns[2] = {
    { PIN_R, false, 0, 0, BUTTONS_HOLD_DEFAULT_MS, false },
    { PIN_S, false, 0, 0, BUTTONS_HOLD_DEFAULT_MS, false },
};

static ButtonEvent g_q[8];
static uint8_t     g_qHead = 0, g_qTail = 0;

static void enqueue(ButtonEvent e) {
    const uint8_t next = (g_qTail + 1) & 7;
    if (next != g_qHead) {
        g_q[g_qTail] = e;
        g_qTail = next;
    }
}

static void serviceBtn(ButtonId id) {
    BtnState& b = g_btns[id];
    const bool     rawNow = (digitalRead(b.pin) == LOW);
    const uint32_t now    = millis();

    // Accept a state transition immediately on the first poll that sees it,
    // as long as we are past the debounce window since the last accepted
    // change. This means a tap as short as one poll gets caught (vs. the
    // older two-poll edge-confirm scheme which dropped quick clicks).
    if (rawNow != b.pressed && (now - b.edgeMs) >= DEBOUNCE_MS) {
        b.pressed = rawNow;
        b.edgeMs  = now;
        if (b.pressed) {
            b.pressMs     = now;
            b.holdEmitted = false;
        } else if (!b.holdEmitted) {
            enqueue(id == BTN_R ? BTN_EV_R_TAP : BTN_EV_S_TAP);
        }
    }

    if (b.pressed && !b.holdEmitted && (now - b.pressMs) >= b.holdMs) {
        b.holdEmitted = true;
        enqueue(id == BTN_R ? BTN_EV_R_HOLD : BTN_EV_S_HOLD);
    }
}

void buttonsInit() {
    pinMode(PIN_R, INPUT_PULLUP);
    pinMode(PIN_S, INPUT_PULLUP);
}

ButtonEvent buttonsPoll() {
    serviceBtn(BTN_R);
    serviceBtn(BTN_S);
    if (g_qHead == g_qTail) return BTN_EV_NONE;
    const ButtonEvent e = g_q[g_qHead];
    g_qHead = (g_qHead + 1) & 7;
    return e;
}

uint32_t buttonHoldMs(ButtonId id) {
    const BtnState& b = g_btns[id];
    if (!b.pressed || b.holdEmitted) return 0;
    uint32_t d = millis() - b.pressMs;
    if (d > b.holdMs) d = b.holdMs;
    return d;
}

void buttonsSetHoldMs(ButtonId id, uint32_t ms) {
    g_btns[id].holdMs = ms;
}

uint32_t buttonsGetHoldMs(ButtonId id) {
    return g_btns[id].holdMs;
}
