#include "AutonomyScreen.h"
#include "Telemetry.h"
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

static constexpr int BAR_X = 380;
static constexpr int BAR_Y = 64;
static constexpr int BAR_W = 500;
static constexpr int BAR_H = 80;

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
    float mean   = 0.0f;
    float stddev = 0.0f;
    telemetryGetKmLStats(mean, stddev);

    const float tankL    = telemetryTankL();
    const float tankCap  = telemetryTankCapacityL();
    const float tankPct  = (tankCap > 0.0001f) ? (tankL / tankCap) : 0.0f;

    const float rangeMid = tankL * mean;
    const float rangeStd = tankL * stddev;
    const int   rangeKm  = (int)(rangeMid + 0.5f);
    const int   plusKm   = (int)(rangeStd + 0.5f);
    const int   minKm    = (int)((mean - 2.0f * stddev) * tankL + 0.5f);
    const int   maxKm    = (int)((mean + 2.0f * stddev) * tankL + 0.5f);

    fillScreenU(COL_BG);

    // --- LEFT: range hero -------------------------------------------------
    drawCenteredInU(0, LEFT_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "AUTONOMIA");

    char heroBuf[8];
    snprintf(heroBuf, sizeof(heroBuf), "%d", rangeKm);
    const uint16_t heroCol = rangeColor(rangeKm);
    const int heroW = measureBitmapText(DSEG7_120, heroBuf);
    const int heroX = (LEFT_W - heroW) / 2;
    drawBitmapText(heroX, 64, DSEG7_120, heroBuf, heroCol);

    char subBuf[20];
    snprintf(subBuf, sizeof(subBuf), "+/- %d km", plusKm);
    drawCenteredInU(0, LEFT_W, 196, 1, 2, COL_AMBER, subBuf);

    // --- DIVIDERS ---------------------------------------------------------
    fillRectU(DIV_X, 16,         DIV_W, FOOTER_DIV - 16, COL_AMBER);
    fillRectU(0,     FOOTER_DIV, USR_W, 2,               COL_AMBER);

    // --- RIGHT: tank gauge ------------------------------------------------
    drawCenteredInU(RIGHT_X, USR_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "TANQUE");

    fillRectU(BAR_X,             BAR_Y,             BAR_W, 4,     COL_AMBER);
    fillRectU(BAR_X,             BAR_Y + BAR_H - 4, BAR_W, 4,     COL_AMBER);
    fillRectU(BAR_X,             BAR_Y,             4,     BAR_H, COL_AMBER);
    fillRectU(BAR_X + BAR_W - 4, BAR_Y,             4,     BAR_H, COL_AMBER);

    // Reserve tick at 15% — visual threshold without doing math.
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

    char tankBuf[16];
    snprintf(tankBuf, sizeof(tankBuf), "%.1f L", tankL);
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
