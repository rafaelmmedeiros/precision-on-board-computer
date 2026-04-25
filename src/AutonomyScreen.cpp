#include "AutonomyScreen.h"
#include "ConsumptionScreen.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts.h"

#include <Arduino.h>
#include <stdio.h>

// --- Layout (mirrors ConsumptionScreen for visual consistency) -------------

static constexpr int LEFT_W      = 340;
static constexpr int DIV_X       = LEFT_W;
static constexpr int DIV_W       = 4;
static constexpr int RIGHT_X     = DIV_X + DIV_W;          // 344

static constexpr int TOP_LABEL_Y = 8;
static constexpr int FOOTER_DIV  = 252;
static constexpr int FOOTER_Y    = 260;

// Tank gauge bar (right half).
static constexpr int BAR_X = 380;
static constexpr int BAR_Y = 64;
static constexpr int BAR_W = 500;
static constexpr int BAR_H = 80;

// --- Vehicle constants -----------------------------------------------------

// Astra 2011 2.0 8V Flex.
static constexpr float TANK_CAP_L = 52.0f;

// --- Bench simulation state ------------------------------------------------
// Real implementation will read the boia on GPIO 33. For now we drain and
// refuel on a tight loop so the gauge animates fast enough to inspect.

static constexpr float SIM_INITIAL_L  = 40.0f;   // start mid-range, not full
static constexpr float SIM_DRAIN_LPS  = 0.30f;   // ~3 min from 40L → 0L
static constexpr float SIM_REFUEL_AT  = 1.0f;    // L threshold for refuel

static float    g_tankL      = SIM_INITIAL_L;
static uint32_t g_simLastMs  = 0;
static bool     g_simInited  = false;

static void simTick() {
    const uint32_t now = millis();
    if (!g_simInited) {
        g_simLastMs  = now;
        g_simInited  = true;
        return;
    }
    const float dt = (now - g_simLastMs) / 1000.0f;
    g_simLastMs    = now;

    g_tankL -= SIM_DRAIN_LPS * dt;
    if (g_tankL < SIM_REFUEL_AT) g_tankL = TANK_CAP_L;
}

// --- Color logic -----------------------------------------------------------

static uint16_t rangeColor(int km) {
    if (km < 100) return COL_HOT;
    if (km > 300) return COL_GOOD;
    return COL_AMBER;
}

static uint16_t tankColor(float pct) {
    if (pct < 0.15f) return COL_HOT;     // reserve
    if (pct < 0.30f) return COL_AMBER;
    return COL_GOOD;
}

// --- Render ----------------------------------------------------------------

void displayAutonomy() {
    simTick();

    float mean   = 0.0f;
    float stddev = 0.0f;
    consumptionGetStats(mean, stddev);

    const float rangeMid = g_tankL * mean;
    const float rangeStd = g_tankL * stddev;
    const int   rangeKm  = (int)(rangeMid + 0.5f);
    const int   plusKm   = (int)(rangeStd + 0.5f);
    const int   minKm    = (int)((mean - 2.0f * stddev) * g_tankL + 0.5f);
    const int   maxKm    = (int)((mean + 2.0f * stddev) * g_tankL + 0.5f);

    const float tankPct  = g_tankL / TANK_CAP_L;

    fillScreenU(COL_BG);

    // --- LEFT: range hero -------------------------------------------------
    drawCenteredInU(0, LEFT_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "AUTONOMIA");

    char heroBuf[8];
    snprintf(heroBuf, sizeof(heroBuf), "%d", rangeKm);
    const uint16_t heroCol = rangeColor(rangeKm);
    const int heroW = measureBitmapText(DSEG7_120, heroBuf);
    const int heroX = (LEFT_W - heroW) / 2;
    drawBitmapText(heroX, 64, DSEG7_120, heroBuf, heroCol);

    // "± 35 km" subtitle in the same slot ConsumptionScreen uses for "Km/l".
    char subBuf[20];
    snprintf(subBuf, sizeof(subBuf), "+/- %d km", plusKm);
    drawCenteredInU(0, LEFT_W, 196, 1, 2, COL_AMBER, subBuf);

    // --- DIVIDERS ---------------------------------------------------------
    fillRectU(DIV_X, 16,         DIV_W, FOOTER_DIV - 16, COL_AMBER);
    fillRectU(0,     FOOTER_DIV, USR_W, 2,               COL_AMBER);

    // --- RIGHT: tank gauge ------------------------------------------------
    drawCenteredInU(RIGHT_X, USR_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "TANQUE");

    // Bar frame (4 px border, hollow center).
    fillRectU(BAR_X,             BAR_Y,             BAR_W, 4,     COL_AMBER);
    fillRectU(BAR_X,             BAR_Y + BAR_H - 4, BAR_W, 4,     COL_AMBER);
    fillRectU(BAR_X,             BAR_Y,             4,     BAR_H, COL_AMBER);
    fillRectU(BAR_X + BAR_W - 4, BAR_Y,             4,     BAR_H, COL_AMBER);

    // Reserve tick at 15% so the driver sees the threshold.
    {
        const int rx = BAR_X + 4 + (int)((BAR_W - 8) * 0.15f);
        fillRectU(rx, BAR_Y - 4, 2, BAR_H + 8, COL_AMBER);
    }

    // Fill.
    {
        const int innerX = BAR_X + 8;
        const int innerY = BAR_Y + 8;
        const int innerW = BAR_W - 16;
        const int innerH = BAR_H - 16;
        const int fillW  = (int)(innerW * tankPct);
        if (fillW > 0) fillRectU(innerX, innerY, fillW, innerH, tankColor(tankPct));
    }

    // Labels below the bar: liters left, percentage right.
    char tankBuf[16];
    snprintf(tankBuf, sizeof(tankBuf), "%.1f L", g_tankL);
    drawCenteredInU(BAR_X, BAR_X + BAR_W / 2, 170, 1, 3, COL_AMBER, tankBuf);

    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d %%", (int)(tankPct * 100.0f + 0.5f));
    drawCenteredInU(BAR_X + BAR_W / 2, BAR_X + BAR_W, 170, 1, 3, COL_AMBER, pctBuf);

    // --- FOOTER: MED / MIN / MAX -----------------------------------------
    char buf[24];
    snprintf(buf, sizeof(buf), "MED %.1f", mean);
    drawCenteredInU(0, 320, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "MIN %d KM", minKm > 0 ? minKm : 0);
    drawCenteredInU(320, 640, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "MAX %d KM", maxKm);
    drawCenteredInU(640, USR_W, FOOTER_Y, 1, 2, COL_AMBER, buf);
}

ResetSet autonomyResets() {
    return { 0, { { nullptr, nullptr }, { nullptr, nullptr } } };
}
