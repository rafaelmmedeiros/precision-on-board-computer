#include "SystemScreen.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts.h"

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
    static const char* const PT_WEEKDAYS[7] = {
        "DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"
    };

    struct tm timeinfo;
    char hhStr[4] = "--";
    char mmStr[4] = "--";
    char dateStr[16] = "--- --/--";
    bool haveTime = getLocalTime(&timeinfo);
    bool colonOn  = true;
    if (haveTime) {
        strftime(hhStr, sizeof(hhStr), "%H", &timeinfo);
        strftime(mmStr, sizeof(mmStr), "%M", &timeinfo);
        const char* wd = PT_WEEKDAYS[timeinfo.tm_wday & 7];
        snprintf(dateStr, sizeof(dateStr), "%s %02d/%02d",
                 wd, timeinfo.tm_mday, timeinfo.tm_mon + 1);
        colonOn = (timeinfo.tm_sec % 2) == 0;
    }

    float volt = simVoltage();
    float tInt = simTempInt();
    float tExt = simTempExt();
    uint16_t vCol = voltageColor(volt);
    uint16_t iCol = tempColor(tInt);
    uint16_t eCol = tempColor(tExt);

    fillScreenU(COL_BG);

    // LEFT HALF — clock (DSEG7 120) on top, date (CGROM 12x24 z3) below.
    // Heights 120 + 72 = 192. Budget 320 − 192 = 128 → 3 gaps ~43.
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

    // RIGHT HALF — voltage (hero 72 + V unit), bar, INT/EXT (hero 48 + C unit).
    // Heights: 72 + 30 + 48 + 48 = 198. Budget 320-198=122 → 5 gaps ~24.
    constexpr int VOLT_Y = 24;
    constexpr int BAR_Y  = 120;
    constexpr int INT_Y  = 174;
    constexpr int EXT_Y  = 246;

    // -- Voltage: "13.5" hero + "V" CGROM 16x32 z2 (32 wide × 64 tall).
    char numBuf[8];
    snprintf(numBuf, sizeof(numBuf), "%.1f", volt);
    const int vNumW  = measureBitmapText(DSEG7_72, numBuf);
    const int vUnitW = fontCellLong(2, 2);   // 32
    constexpr int V_GAP = 10;
    const int vTotalW = vNumW + V_GAP + vUnitW;
    const int vStartX = RIGHT_START + (RIGHT_END - RIGHT_START - vTotalW) / 2;
    drawBitmapText(vStartX, VOLT_Y, DSEG7_72, numBuf, vCol);
    // Align unit vertically centered against the hero (hero 72 tall, unit 64).
    drawTextU(vStartX + vNumW + V_GAP, VOLT_Y + (72 - 64) / 2, 2, 2, vCol, "V");

    // -- Voltage bar (unchanged layout).
    constexpr int BAR_W = 400;
    constexpr int BAR_H = 30;
    constexpr int BAR_X = (RIGHT_START + RIGHT_END - BAR_W) / 2;
    fillRectU(BAR_X,             BAR_Y,             BAR_W, 3,     COL_AMBER);
    fillRectU(BAR_X,             BAR_Y + BAR_H - 3, BAR_W, 3,     COL_AMBER);
    fillRectU(BAR_X,             BAR_Y,             3,     BAR_H, COL_AMBER);
    fillRectU(BAR_X + BAR_W - 3, BAR_Y,             3,     BAR_H, COL_AMBER);
    float pct = (volt - 10.0f) / 6.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    const int fw = (int)((BAR_W - 12) * pct);
    fillRectU(BAR_X + 6, BAR_Y + 6, fw, BAR_H - 12, vCol);

    // -- Temperatures: "INT" CGROM + "XX.X" hero 48 + "C" CGROM. Same for EXT.
    // CGROM 12x24 z2 = 24w × 48t per char.
    auto drawTempRow = [&](int y, const char* label, float value, uint16_t col) {
        char tb[8];
        snprintf(tb, sizeof(tb), "%.1f", value);
        constexpr int CELL   = 12 * 2;              // 24
        constexpr int LBL_W  = 3 * CELL;            // "INT"/"EXT" = 72
        constexpr int UNIT_W = CELL;                // "C" = 24
        constexpr int GAP    = 16;
        const int numW   = measureBitmapText(DSEG7_48, tb);
        const int totalW = LBL_W + GAP + numW + GAP + UNIT_W;
        const int x0     = RIGHT_START + (RIGHT_END - RIGHT_START - totalW) / 2;
        drawTextU(x0, y, 1, 2, COL_AMBER, label);
        drawBitmapText(x0 + LBL_W + GAP, y, DSEG7_48, tb, col);
        drawTextU(x0 + LBL_W + GAP + numW + GAP, y, 1, 2, col, "C");
    };
    drawTempRow(INT_Y, "INT", tInt, iCol);
    drawTempRow(EXT_Y, "EXT", tExt, eCol);

    (void)LEFT_MID;   // kept for future hand-aligned elements
    (void)RIGHT_MID;
}
