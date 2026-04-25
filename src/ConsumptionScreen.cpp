#include "ConsumptionScreen.h"
#include "Layout.h"
#include "Telemetry.h"
#include "TripLog.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts.h"

#include <Arduino.h>
#include <stdio.h>

using namespace pobc;

// --- Layout constants ------------------------------------------------------
// Shared rails (TOP_LABEL_Y, FOOTER_DIV, FOOTER_Y, LEFT_W, DIV_X, DIV_W,
// RIGHT_X, DIV_Y_TOP) come from Layout.h.

static constexpr int CHART_X      = RIGHT_X + 60;          // leave Y-label room
static constexpr int CHART_RIGHT  = USR_W - 18;
static constexpr int CHART_TOP    = 64;
static constexpr int CHART_BOTTOM = 220;
static constexpr int CHART_W      = CHART_RIGHT - CHART_X;
static constexpr int CHART_H      = CHART_BOTTOM - CHART_TOP;

static constexpr float SCALE_MAX = 20.0f;   // top of Y axis (km/L)

// --- Helpers ---------------------------------------------------------------

static int valueToY(float v) {
    if (v < 0)         v = 0;
    if (v > SCALE_MAX) v = SCALE_MAX;
    return CHART_BOTTOM - (int)(v / SCALE_MAX * (float)CHART_H);
}

static void drawGridLine(float v) {
    const int y = valueToY(v);
    for (int x = CHART_X; x < CHART_RIGHT; x += 10) {
        fillRectU(x, y, 4, 1, COL_AMBER);
    }
    char lbl[4];
    snprintf(lbl, sizeof(lbl), "%d", (int)v);
    drawTextU(CHART_X - 28, y - 8, 0, 1, COL_AMBER, lbl);
}

// --- Render ----------------------------------------------------------------

void displayConsumption() {
    const float instant = telemetryKmL();
    const bool  cut     = telemetryFuelCut();
    const float tripKm  = telemetryTripKm();
    const float tripL   = telemetryTripL();
    const uint32_t tripSec = telemetryTripSec();
    const float tripAvg = (tripL > 0.0001f) ? (tripKm / tripL) : instant;
    const int   tripH   = (int)(tripSec / 3600U);
    const int   tripM   = (int)((tripSec / 60U) % 60U);

    fillScreenU(COL_BG);

    // --- LEFT SIDE: hero current consumption ------------------------------
    drawCenteredInU(0, LEFT_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "INST");

    if (cut) {
        // DFCO — Deceleration Fuel Cut-Off. No injector pulses, no fuel,
        // best km/L the engine can do. Show "CORTE" in green at the hero
        // slot; subtitle echoes the state.
        drawCenteredInU(0, LEFT_W, 76, 2, 3, COL_GOOD, "CORTE");
        drawCenteredInU(0, LEFT_W, 196, 1, 2, COL_GOOD, "DFCO");
    } else {
        char heroBuf[8];
        snprintf(heroBuf, sizeof(heroBuf), "%.1f", instant);
        const uint16_t heroCol = consumptionColor(instant);
        const int heroW = measureBitmapText(DSEG7_120, heroBuf);
        const int heroX = (LEFT_W - heroW) / 2;
        drawBitmapText(heroX, 64, DSEG7_120, heroBuf, heroCol);

        drawCenteredInU(0, LEFT_W, 196, 1, 2, COL_AMBER, "Km/l");
    }

    // --- DIVIDERS ---------------------------------------------------------
    fillRectU(DIV_X, DIV_Y_TOP, DIV_W, FOOTER_DIV - DIV_Y_TOP, COL_AMBER);
    fillRectU(0,     FOOTER_DIV, USR_W, 2,                     COL_AMBER);

    // --- RIGHT SIDE: history bar chart ------------------------------------
    drawCenteredInU(RIGHT_X, USR_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "5m | Km/l");

    drawGridLine(5.0f);
    drawGridLine(10.0f);
    drawGridLine(15.0f);
    drawGridLine(20.0f);

    fillRectU(CHART_X, CHART_BOTTOM, CHART_W, 2, COL_AMBER);

    // Bars: index 0 (live) is always drawn; index 1..histCount grow in as
    // 5-min slots close. The chart starts empty on every power-on.
    const int slot    = CHART_W / TELEMETRY_HIST_N;
    const int barW    = (slot * 7) / 10;
    const int barPad  = (slot - barW) / 2;
    const int lastBar = telemetryHistCount();
    for (int i = 0; i <= lastBar; ++i) {
        const float v  = telemetryHistAt(i);
        const int   by = valueToY(v);
        const int   bh = CHART_BOTTOM - by;
        const int   bx = CHART_X + i * slot + barPad;
        const uint16_t bc = consumptionColor(v);
        if (bh > 0) fillRectU(bx, by, barW, bh, bc);
    }

    // Live indicator: small downward triangle above bar 0 so the eye
    // instantly knows that one is the moving target.
    {
        const int bx = CHART_X + 0 * slot + barPad;
        const int markX = bx + barW / 2;
        const int markY = CHART_TOP - 8;
        for (int k = 0; k < 4; ++k) {
            fillRectU(markX - 3 + k, markY + k, 7 - 2 * k, 1, COL_AMBER);
        }
    }

    drawTextU(CHART_X + 0 * slot,                          CHART_BOTTOM + 8, 0, 1, COL_AMBER, "AGORA");
    drawTextU(CHART_X + 4 * slot - 8,                      CHART_BOTTOM + 8, 0, 1, COL_AMBER, "-20");
    drawTextU(CHART_X + 8 * slot - 8,                      CHART_BOTTOM + 8, 0, 1, COL_AMBER, "-40");
    drawTextU(CHART_X + (TELEMETRY_HIST_N - 1) * slot - 8, CHART_BOTTOM + 8, 0, 1, COL_AMBER, "-60");

    // --- FOOTER: trip summary --------------------------------------------
    char buf[24];
    snprintf(buf, sizeof(buf), "MED %.1f", tripAvg);
    drawTextU(20, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "%d KM", (int)tripKm);
    drawCenteredInU(260, 540, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "%.1f L", tripL);
    drawCenteredInU(540, 760, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "%d:%02d", tripH, tripM);
    drawTextU(USR_W - 140, FOOTER_Y, 1, 2, COL_AMBER, buf);
}

ResetSet consumptionResets() {
    // "Encerrar" instead of "Reset" because the action does more than zero
    // the counters — it snapshots the trip into the persistent log first,
    // then resets. The user's "carro que manda" principle (memory) prefers
    // automatic trip closure on long ignition-off; until that signal is
    // wired, this is the manual override.
    return {
        1,
        {
            { "Encerrar viagem", &tripLogFinishCurrentTrip },
            { nullptr,            nullptr                  },
        }
    };
}
