#include "SystemScreen.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts_DSEG7.h"

#include <Arduino.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

// --- Layout constants ------------------------------------------------------
// Vertical divider centered on the 960-wide display.
static constexpr int DIV_X = 478;
static constexpr int DIV_W = 4;
static constexpr int DIV_Y = 30;
static constexpr int DIV_H = 260;

// Content area boundaries (user.X).
static constexpr int LEFT_START  = 0;
static constexpr int LEFT_END    = DIV_X;               // 478
static constexpr int RIGHT_START = DIV_X + DIV_W;       // 482
static constexpr int RIGHT_END   = USR_W;               // 960

// Pixel center of each half — used for sanity when aligning by hand.
static constexpr int LEFT_MID  = (LEFT_START + LEFT_END)  / 2;   // 239
static constexpr int RIGHT_MID = (RIGHT_START + RIGHT_END) / 2;  // 721

// --- Simulated values (bench demo — sensors not yet wired) -----------------

static float simVoltage() {
    float t = millis() / 1000.0f;
    return 13.25f + 2.25f * sinf(t * 2.0f * (float)M_PI / 30.0f);
}

static float simTempInt() {
    float t = millis() / 1000.0f;
    return 22.5f + 17.5f * sinf(t * 2.0f * (float)M_PI / 24.0f + 1.2f);
}

static float simTempExt() {
    float t = millis() / 1000.0f;
    return 20.0f + 25.0f * sinf(t * 2.0f * (float)M_PI / 36.0f + 2.4f);
}

// --- Screen render ---------------------------------------------------------

void displaySystem() {
    struct tm timeinfo;
    char hhStr[4] = "--";
    char mmStr[4] = "--";
    char dateStr[8] = "--/--";
    bool haveTime = getLocalTime(&timeinfo);
    bool colonOn  = true;
    if (haveTime) {
        strftime(hhStr,   sizeof(hhStr),   "%H",    &timeinfo);
        strftime(mmStr,   sizeof(mmStr),   "%M",    &timeinfo);
        strftime(dateStr, sizeof(dateStr), "%d/%m", &timeinfo);
        colonOn = (timeinfo.tm_sec % 2) == 0;
    }

    float volt = simVoltage();
    float tInt = simTempInt();
    float tExt = simTempExt();
    uint16_t vCol = voltageColor(volt);
    uint16_t iCol = tempColor(tInt);
    uint16_t eCol = tempColor(tExt);

    fillScreenU(COL_BG);

    // LEFT HALF — time (DSEG7 120px) on top, date (CGROM 12x24 z3) below.
    // Line height 120 + date block 72. Budget 320 − 192 = 128 → ~43px gaps.
    // Measure with valid glyphs: the font has only digits, ':' and a few punct.
    // "88" and "--" both measure 2 × digit advance, so the layout stays stable
    // whether we're showing real time or the boot placeholder.
    const int hhW    = measureBitmapText(DSEG7_120, hhStr);
    const int mmW    = measureBitmapText(DSEG7_120, mmStr);
    const int colonW = measureBitmapText(DSEG7_120, ":");
    const int timeW  = hhW + colonW + mmW;
    const int timeX  = LEFT_START + (LEFT_END - LEFT_START - timeW) / 2;
    constexpr int TIME_Y = 43;
    constexpr int DATE_Y = 206;

    drawBitmapText(timeX, TIME_Y, DSEG7_120, hhStr, COL_AMBER);
    if (colonOn) {
        drawBitmapText(timeX + hhW, TIME_Y, DSEG7_120, ":", COL_AMBER);
    }
    drawBitmapText(timeX + hhW + colonW, TIME_Y, DSEG7_120, mmStr, COL_AMBER);

    drawCenteredInU(LEFT_START, LEFT_END, DATE_Y, 1, 3, COL_AMBER, dateStr);

    // DIVIDER (exact screen center).
    fillRectU(DIV_X, DIV_Y, DIV_W, DIV_H, COL_AMBER);

    // RIGHT HALF — voltage value, bar, INT, EXT. Balanced five-gap vertical.
    char buf[12];

    // Voltage value 16x32 zoom 2 = 160 × 64. Centered above the bar.
    snprintf(buf, sizeof(buf), "%.1fV", volt);
    drawCenteredInU(RIGHT_START, RIGHT_END, 26, 2, 2, vCol, buf);

    // Voltage bar (10..16 V range), 400 wide centered, 30 tall.
    constexpr int BAR_W = 400;
    constexpr int BAR_H = 30;
    constexpr int BAR_X = (RIGHT_START + RIGHT_END - BAR_W) / 2;   // 521
    constexpr int BAR_Y = 116;
    fillRectU(BAR_X,               BAR_Y,               BAR_W, 3,     COL_AMBER);
    fillRectU(BAR_X,               BAR_Y + BAR_H - 3,   BAR_W, 3,     COL_AMBER);
    fillRectU(BAR_X,               BAR_Y,               3,     BAR_H, COL_AMBER);
    fillRectU(BAR_X + BAR_W - 3,   BAR_Y,               3,     BAR_H, COL_AMBER);
    float pct = (volt - 10.0f) / 6.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    int fw = (int)((BAR_W - 12) * pct);
    fillRectU(BAR_X + 6, BAR_Y + 6, fw, BAR_H - 12, vCol);

    // Temperatures. "INT" label amber (3 chars) + one-char gap + "XX.XC" value.
    // 12x24 zoom 2 = 24 × 48 per char.
    constexpr int T_CELL  = 12 * 2;                      // 24 (user.X per char)
    constexpr int T_LABEL = 3 * T_CELL;                  // 72
    constexpr int T_GAP   = T_CELL;                      // 24
    constexpr int T_VALUE = 5 * T_CELL;                  // 120
    constexpr int T_GROUP = T_LABEL + T_GAP + T_VALUE;   // 216
    constexpr int T_LABEL_X = (RIGHT_START + RIGHT_END - T_GROUP) / 2;
    constexpr int T_VALUE_X = T_LABEL_X + T_LABEL + T_GAP;

    drawTextU(T_LABEL_X, 172, 1, 2, COL_AMBER, "INT");
    snprintf(buf, sizeof(buf), "%.1fC", tInt);
    drawTextU(T_VALUE_X, 172, 1, 2, iCol, buf);

    drawTextU(T_LABEL_X, 246, 1, 2, COL_AMBER, "EXT");
    snprintf(buf, sizeof(buf), "%.1fC", tExt);
    drawTextU(T_VALUE_X, 246, 1, 2, eCol, buf);

    (void)LEFT_MID;   // kept for future hand-aligned elements
    (void)RIGHT_MID;
}
