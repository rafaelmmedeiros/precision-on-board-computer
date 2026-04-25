#include "ConsumptionScreen.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

// --- Layout constants ------------------------------------------------------
// Vertical bands: top label / hero+chart / x-axis / footer.
static constexpr int LEFT_W      = 340;
static constexpr int DIV_X       = LEFT_W;
static constexpr int DIV_W       = 4;
static constexpr int RIGHT_X     = DIV_X + DIV_W;          // 344

static constexpr int TOP_LABEL_Y = 8;
static constexpr int FOOTER_DIV  = 252;
static constexpr int FOOTER_Y    = 260;

// Chart bounding box (right side).
static constexpr int CHART_X      = RIGHT_X + 60;          // leave room for Y labels
static constexpr int CHART_RIGHT  = USR_W - 18;
static constexpr int CHART_TOP    = 64;
static constexpr int CHART_BOTTOM = 220;
static constexpr int CHART_W      = CHART_RIGHT - CHART_X;
static constexpr int CHART_H      = CHART_BOTTOM - CHART_TOP;

// Consumption scale (km/L).
static constexpr float SCALE_MAX = 20.0f;

// History size and slot duration. Demo: 30 s/slot so the shift is visible
// at the bench. Real Phase 2 firmware: 5 * 60 * 1000.
static constexpr int      HIST_N         = 12;
static constexpr uint32_t HIST_PERIOD_MS = 30UL * 1000UL;

// --- State -----------------------------------------------------------------

static float    g_history[HIST_N] = {0};   // [0]=live, [1..populated]=locked
static int      g_populated   = 0;          // count of locked bars filled in [1..N-1]
static uint32_t g_periodStart = 0;
static float    g_periodSum   = 0.0f;
static int      g_periodN     = 0;

// Trip integrator — constant 80 km/h sim, consumption varies. Liters and km
// accumulate every frame so the avg km/L is a true running mean.
static constexpr float SIM_SPEED_KMH = 80.0f;
static uint32_t g_tripStartMs = 0;
static uint32_t g_tripLastMs  = 0;
static float    g_tripKm      = 0.0f;
static float    g_tripL       = 0.0f;
static bool     g_tripInited  = false;

// --- Helpers ---------------------------------------------------------------

static uint16_t consumptionColor(float kmL) {
    if (kmL < 10.0f) return COL_HOT;
    if (kmL > 14.0f) return COL_GOOD;
    return COL_AMBER;
}

static float simInstant() {
    // Slow base wave (city/highway alternation) + a faster ripple to make
    // the live bar visibly move.
    float t = millis() / 1000.0f;
    float base   = 12.0f + 4.0f * sinf(t * 2.0f * (float)M_PI / 18.0f);
    float ripple = 1.2f * sinf(t * 1.7f);
    return base + ripple;
}

static void seedTripOnce() {
    if (g_tripInited) return;
    g_tripStartMs = millis();
    g_tripLastMs  = g_tripStartMs;
    g_periodStart = g_tripStartMs;
    g_tripInited  = true;
}

static void tickHistory(float instant) {
    g_history[0] = instant;
    g_periodSum += instant;
    g_periodN   += 1;
    if (millis() - g_periodStart >= HIST_PERIOD_MS && g_periodN > 0) {
        const float avg = g_periodSum / (float)g_periodN;
        for (int i = HIST_N - 1; i >= 2; --i) g_history[i] = g_history[i - 1];
        g_history[1] = avg;
        if (g_populated < HIST_N - 1) g_populated++;
        g_periodSum  = 0.0f;
        g_periodN    = 0;
        g_periodStart = millis();
    }
}

static int valueToY(float v) {
    if (v < 0)         v = 0;
    if (v > SCALE_MAX) v = SCALE_MAX;
    return CHART_BOTTOM - (int)(v / SCALE_MAX * (float)CHART_H);
}

// Dotted horizontal gridline at consumption level v.
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
    seedTripOnce();

    const float instant = simInstant();
    tickHistory(instant);

    // Trip integrator — constant 80 km/h sim. Per-frame Δkm = v · Δt;
    // Δlitros = Δkm / consumo_atual. Avg km/L emerges as tripKm / tripL.
    const uint32_t now = millis();
    const float dtSec  = (now - g_tripLastMs) / 1000.0f;
    g_tripLastMs       = now;
    const float dKm = SIM_SPEED_KMH * dtSec / 3600.0f;
    g_tripKm += dKm;
    if (instant > 0.1f) g_tripL += dKm / instant;
    const float tripAvg = (g_tripL > 0.0001f) ? (g_tripKm / g_tripL) : instant;
    const int   tripSec = (int)((now - g_tripStartMs) / 1000UL);
    const int   tripH   = tripSec / 3600;
    const int   tripM   = (tripSec / 60) % 60;

    fillScreenU(COL_BG);

    // --- LEFT SIDE: hero current consumption ------------------------------
    drawCenteredInU(0, LEFT_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "INST");

    char heroBuf[8];
    snprintf(heroBuf, sizeof(heroBuf), "%.1f", instant);
    const uint16_t heroCol = consumptionColor(instant);
    const int heroW = measureBitmapText(DSEG7_120, heroBuf);
    const int heroX = (LEFT_W - heroW) / 2;
    drawBitmapText(heroX, 64, DSEG7_120, heroBuf, heroCol);

    drawCenteredInU(0, LEFT_W, 196, 1, 2, COL_AMBER, "KM/L");

    // --- DIVIDERS ---------------------------------------------------------
    fillRectU(DIV_X,    16, DIV_W,                FOOTER_DIV - 16, COL_AMBER);
    fillRectU(0,        FOOTER_DIV, USR_W,        2,               COL_AMBER);

    // --- RIGHT SIDE: history bar chart ------------------------------------
    drawCenteredInU(RIGHT_X, USR_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "5M | KM/L");

    // Gridlines + Y labels.
    drawGridLine(5.0f);
    drawGridLine(10.0f);
    drawGridLine(15.0f);
    drawGridLine(20.0f);

    // X-axis baseline.
    fillRectU(CHART_X, CHART_BOTTOM, CHART_W, 2, COL_AMBER);

    // Bars. Slot pitch = total / N. Bar takes 70% of its slot, gap is the rest.
    // Index 0 (live) is always drawn; index 1..g_populated grow in as 5-min
    // slots close. The chart starts empty on every power-on (new trip).
    const int slot   = CHART_W / HIST_N;
    const int barW   = (slot * 7) / 10;
    const int barPad = (slot - barW) / 2;
    const int lastBar = g_populated;   // bars [0..lastBar] are valid
    for (int i = 0; i <= lastBar; ++i) {
        const float v  = g_history[i];
        const int   by = valueToY(v);
        const int   bh = CHART_BOTTOM - by;
        const int   bx = CHART_X + i * slot + barPad;
        const uint16_t bc = consumptionColor(v);
        if (bh > 0) fillRectU(bx, by, barW, bh, bc);
    }

    // Live indicator: a small triangle/notch above bar 0 so the eye instantly
    // knows that one is the moving target.
    {
        const int bx = CHART_X + 0 * slot + barPad;
        const int markX = bx + barW / 2;
        const int markY = CHART_TOP - 8;
        // 7-px wide downward triangle approximated with 4 stacked rects.
        for (int k = 0; k < 4; ++k) {
            fillRectU(markX - 3 + k, markY + k, 7 - 2 * k, 1, COL_AMBER);
        }
    }

    // X-axis labels: AGORA at bar 0; -20/-40/-60 at later positions.
    drawTextU(CHART_X + 0 * slot,                CHART_BOTTOM + 8, 0, 1, COL_AMBER, "AGORA");
    drawTextU(CHART_X + 4 * slot - 8,            CHART_BOTTOM + 8, 0, 1, COL_AMBER, "-20");
    drawTextU(CHART_X + 8 * slot - 8,            CHART_BOTTOM + 8, 0, 1, COL_AMBER, "-40");
    drawTextU(CHART_X + (HIST_N - 1) * slot - 8, CHART_BOTTOM + 8, 0, 1, COL_AMBER, "-60");

    // --- FOOTER: trip summary, declutter pass ----------------------------
    // 4 cells, no redundant unit suffixes ("KM/L" implied by the screen,
    // "TRIP" prefix dropped since the unit makes it obvious).
    char buf[24];
    snprintf(buf, sizeof(buf), "MED %.1f", tripAvg);
    drawTextU(20, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "%d KM", (int)g_tripKm);
    drawCenteredInU(260, 540, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "%.1f L", g_tripL);
    drawCenteredInU(540, 760, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "%d:%02d", tripH, tripM);
    drawTextU(USR_W - 140, FOOTER_Y, 1, 2, COL_AMBER, buf);
}
