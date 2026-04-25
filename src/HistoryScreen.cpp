#include "HistoryScreen.h"
#include "Layout.h"
#include "TripLog.h"
#include "Tft.h"

#include <Arduino.h>
#include <stdio.h>
#include <time.h>

using namespace pobc;

// Title and footer rails come from Layout.h. Chart geometry is
// screen-specific (full-width chart, no left/right pane split).

static constexpr int CHART_TOP    = 56;
static constexpr int CHART_BOTTOM = 220;
static constexpr int CHART_X      = 60;
static constexpr int CHART_RIGHT  = USR_W - 20;
static constexpr int CHART_W      = CHART_RIGHT - CHART_X;
static constexpr int CHART_H      = CHART_BOTTOM - CHART_TOP;

static constexpr float SCALE_MAX = 20.0f;   // top of Y axis (km/L) — same as Consumption

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

// Find the index of the highest avg km/L in the log. -1 if log empty.
static int bestIndex() {
    const int n = tripLogCount();
    if (n == 0) return -1;
    int   best = 0;
    float top  = tripLogAt(0).avgKmL;
    for (int i = 1; i < n; ++i) {
        const float v = tripLogAt(i).avgKmL;
        if (v > top) { top = v; best = i; }
    }
    return best;
}

static void formatDate(char* out, size_t cap, uint32_t unixSec) {
    if (unixSec == 0) { snprintf(out, cap, "--/--"); return; }
    const time_t t = (time_t)unixSec;
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, cap, "%02d/%02d", tm.tm_mday, tm.tm_mon + 1);
}

// --- Render ----------------------------------------------------------------

void displayHistory() {
    fillScreenU(COL_BG);

    const int count = tripLogCount();

    // --- Title row -------------------------------------------------------
    drawTextU(20, TOP_LABEL_Y, 1, 2, COL_AMBER, "HISTORICO");
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "%d viagem%s", count, count == 1 ? "" : "s");
    const int cellW = 12 * 2;   // font 1 z2 cell width
    const int cw    = (int)strlen(countBuf) * cellW;
    drawTextU(USR_W - cw - 20, TOP_LABEL_Y, 1, 2, COL_AMBER, countBuf);

    // --- Empty state ------------------------------------------------------
    if (count == 0) {
        drawCenteredU("SEM VIAGENS REGISTRADAS", 110, 1, 3, COL_AMBER);
        drawCenteredU("Encerre uma viagem na tela de Consumo", 200, 1, 1, COL_AMBER);
        return;
    }

    const int best     = bestIndex();
    const int bestSlot = (best >= 0) ? (count - 1 - best) : -1;   // newest-on-left mapping

    // --- Chart ------------------------------------------------------------
    drawGridLine(5.0f);
    drawGridLine(10.0f);
    drawGridLine(15.0f);
    drawGridLine(20.0f);

    // Baseline.
    fillRectU(CHART_X, CHART_BOTTOM, CHART_W, 2, COL_AMBER);

    // Bars: slot pitch fixed at TRIP_LOG_MAX. Newest sits on the LEFT to
    // match ConsumptionScreen's convention (AGORA on the left, older slots
    // extend rightward). Slot 0 = most recent trip, slot count-1 = oldest
    // still in the log; slots count..N_MAX-1 stay empty.
    const int slot   = CHART_W / TRIP_LOG_MAX;
    const int barW   = (slot * 7) / 10;
    const int barPad = (slot - barW) / 2;

    for (int i = 0; i < count; ++i) {
        // tripLogAt is oldest-first; flip so log[count-1] (newest) → slot 0.
        const TripRecord& r = tripLogAt(count - 1 - i);
        const int slotIdx = i;
        const int by = valueToY(r.avgKmL);
        const int bh = CHART_BOTTOM - by;
        const int bx = CHART_X + slotIdx * slot + barPad;
        const uint16_t bc = consumptionColor(r.avgKmL);
        if (bh > 0) fillRectU(bx, by, barW, bh, bc);

        // Highlight outline on the best-of-window bar.
        if (i == bestSlot && bh > 0) {
            fillRectU(bx - 2, by - 2, barW + 4, 2, COL_AMBER);
            fillRectU(bx - 2, CHART_BOTTOM,    barW + 4, 2, COL_AMBER);
            fillRectU(bx - 2, by - 2, 2, bh + 4, COL_AMBER);
            fillRectU(bx + barW, by - 2, 2, bh + 4, COL_AMBER);
        }

        // Date label below the bar (DD/MM).
        char dateBuf[8];
        formatDate(dateBuf, sizeof(dateBuf), r.startUnixSec);
        const int dateW = 8 * (int)strlen(dateBuf);   // font 0 z1 = 8 wide
        const int dateX = bx + (barW - dateW) / 2;
        drawTextU(dateX, CHART_BOTTOM + 6, 0, 1, COL_AMBER, dateBuf);
    }

    // --- Footer divider --------------------------------------------------
    fillRectU(0, FOOTER_DIV, USR_W, 2, COL_AMBER);

    // --- Footer: melhor + ultima -----------------------------------------
    // Two cells, each ~480 px wide. Keep strings short — anything wider
    // than the cell will get clipped to the cell's left edge by
    // drawCenteredInU and overflow rightwards into the other cell or off
    // the right edge of the panel, which the LT7680 renders unpredictably.
    char buf[40];
    {
        const TripRecord& b = tripLogAt(best);
        snprintf(buf, sizeof(buf), "MELHOR %.1f", b.avgKmL);
        drawCenteredInU(0, USR_W / 2, FOOTER_Y, 1, 2, COL_GOOD, buf);
    }
    {
        const TripRecord& last = tripLogAt(count - 1);
        snprintf(buf, sizeof(buf), "ULTIMA %.1f", last.avgKmL);
        drawCenteredInU(USR_W / 2, USR_W, FOOTER_Y, 1, 2, COL_AMBER, buf);
    }
}

// --- Reset hooks ----------------------------------------------------------

ResetSet historyResets() {
    return {
        1,
        {
            { "Apagar historico", &tripLogClear },
            { nullptr,             nullptr      },
        }
    };
}
