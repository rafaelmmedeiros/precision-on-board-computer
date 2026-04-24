#include "SystemScreen.h"
#include "Tft.h"

#include <Arduino.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

// --- Layout constants ------------------------------------------------------
// Divider line centered on the 960-wide display (pixels 478..481).
static constexpr int DIV_X  = 478;
static constexpr int DIV_W  = 4;
static constexpr int DIV_Y  = 30;
static constexpr int DIV_H  = 260;

// Content area boundaries (user X).
static constexpr int LEFT_START  = 0;
static constexpr int LEFT_END    = DIV_X;               // 478
static constexpr int RIGHT_START = DIV_X + DIV_W;       // 482
static constexpr int RIGHT_END   = USR_W;               // 960

// --- Simulated values (bench demo — sensors not yet wired) -----------------

static float simVoltage() {
    float t = millis() / 1000.0f;
    // 11.0..15.5 V over 30 s — cycles through red/amber/green/red bands.
    return 13.25f + 2.25f * sinf(t * 2.0f * (float)M_PI / 30.0f);
}

static float simTempInt() {
    float t = millis() / 1000.0f;
    // 5..40 °C over 24 s — crosses both 10 °C and 30 °C thresholds.
    return 22.5f + 17.5f * sinf(t * 2.0f * (float)M_PI / 24.0f + 1.2f);
}

static float simTempExt() {
    float t = millis() / 1000.0f;
    // -5..45 °C over 36 s — swings across all three color ranges, offset
    // from the internal temp so values don't march in lockstep.
    return 20.0f + 25.0f * sinf(t * 2.0f * (float)M_PI / 36.0f + 2.4f);
}

// --- Screen render ---------------------------------------------------------

void displaySystem() {
    struct tm timeinfo;
    char timeStr[8] = "--:--";
    char dateStr[8] = "--/--";
    if (getLocalTime(&timeinfo)) {
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
        strftime(dateStr, sizeof(dateStr), "%d/%m", &timeinfo);
    }

    float volt = simVoltage();
    float tInt = simTempInt();
    float tExt = simTempExt();
    uint16_t vCol = voltageColor(volt);
    uint16_t iCol = tempColor(tInt);
    uint16_t eCol = tempColor(tExt);

    fillScreenU(COL_BG);

    // LEFT HALF — time (big) + date (medium), vertically balanced.
    // time: 16×32 zoom 3 → 480×48 glyph block, fills left half
    // date: 12×24 zoom 3 → 360×36 glyph block
    drawCenteredInU(LEFT_START, LEFT_END,  80, 2, 3, COL_AMBER, timeStr);
    drawCenteredInU(LEFT_START, LEFT_END, 208, 1, 3, COL_AMBER, dateStr);

    // DIVIDER — dead center of the screen.
    fillRectU(DIV_X, DIV_Y, DIV_W, DIV_H, COL_AMBER);

    // RIGHT HALF — voltage value, voltage bar, INT temp, EXT temp.
    char buf[12];

    // Voltage value (dominant element). 16×32 zoom 2 → 320×32.
    snprintf(buf, sizeof(buf), "%.1fV", volt);
    drawCenteredInU(RIGHT_START, RIGHT_END, 42, 2, 2, vCol, buf);

    // Voltage bar (10..16 V, 6 V span). 400 wide, centered in right half.
    const int barW = 400;
    const int barH = 30;
    const int barX = RIGHT_START + (RIGHT_END - RIGHT_START - barW) / 2;
    const int barY = 116;
    fillRectU(barX,            barY,            barW, 3,    COL_AMBER);
    fillRectU(barX,            barY + barH - 3, barW, 3,    COL_AMBER);
    fillRectU(barX,            barY,            3,    barH, COL_AMBER);
    fillRectU(barX + barW - 3, barY,            3,    barH, COL_AMBER);
    float pct = (volt - 10.0f) / 6.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    int fw = (int)((barW - 12) * pct);
    fillRectU(barX + 6, barY + 6, fw, barH - 12, vCol);

    // Temperatures — "INT  25.3C" pattern. 12×24 zoom 2 → 48 px per char.
    // Group = "INT" (3×48) + one-char gap (48) + "XX.XC" (5×48) = 432 px.
    const int tCellW   = fontCellLong(1, 2);                // 48
    const int labelW   = 3 * tCellW;                         // 144
    const int gap      = tCellW;                             // 48
    const int valueW   = 5 * tCellW;                         // 240
    const int groupW   = labelW + gap + valueW;              // 432
    const int tLabelX  = RIGHT_START + (RIGHT_END - RIGHT_START - groupW) / 2;
    const int tValueX  = tLabelX + labelW + gap;

    drawTextU(tLabelX, 188, 1, 2, COL_AMBER, "INT");
    snprintf(buf, sizeof(buf), "%.1fC", tInt);
    drawTextU(tValueX, 188, 1, 2, iCol, buf);

    drawTextU(tLabelX, 254, 1, 2, COL_AMBER, "EXT");
    snprintf(buf, sizeof(buf), "%.1fC", tExt);
    drawTextU(tValueX, 254, 1, 2, eCol, buf);
}
