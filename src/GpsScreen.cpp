#include "GpsScreen.h"
#include "GpsModule.h"
#include "Layout.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace pobc;

// Layout rails come from Layout.h. Screen-specific geometry below.
//
// Right column carries three rows: grade, speed, heading. Heights are 72
// (DSEG7_72) / 72 / 64 (CGROM 16x32 z2) = 208 px, which has to live inside the
// DIV_Y_TOP(16)..FOOTER_DIV(252) band — 236 px. Hence the tight 8 px gaps: a
// 96 px heading row would have crossed the footer divider.
static constexpr int ROW_GRADE_Y = 20;
static constexpr int ROW_SPEED_Y = 100;
static constexpr int ROW_HDG_Y   = 180;

static constexpr int HERO_Y      = 76;   // altitude hero top edge
static constexpr int HERO_MARGIN = 8;    // keep the hero off the column edges

// --- Color logic -----------------------------------------------------------
// Screen-local, following the rangeColor/tankColor precedent in
// AutonomyScreen.cpp: only this view renders these quantities, so they do not
// belong in Tft.h's shared set.

// Climbing costs fuel (hot), descending tends toward DFCO (cold). Matches the
// existing colour language rather than inventing a new one.
static uint16_t gradeColor(float pct) {
    if (isnan(pct))   return COL_AMBER;
    if (pct >  3.0f)  return COL_HOT;
    if (pct < -3.0f)  return COL_COLD;
    return COL_AMBER;
}

static uint16_t fixColor(bool hasFix, uint8_t sats, float hdop) {
    if (!hasFix)                      return COL_HOT;
    if (sats >= 6 && hdop <= 2.0f)    return COL_GOOD;
    if (sats >= 4)                    return COL_AMBER;
    return COL_HOT;
}

// --- Render ----------------------------------------------------------------

// Label (CGROM) + hero (DSEG7_72) + unit (CGROM), centred as a group in the
// right column. Same construction as SystemScreen's temperature rows.
static void drawRow(int y, const char* label, const char* value,
                    const char* unit, uint16_t col) {
    constexpr int GAP = 16;

    const int cell  = fontCellLong(1, 2);    // CGROM 12x24 z2 → 24 px per char
    const int textH = fontCellShort(1, 2);   // → 48 px tall
    const int lblW  = (int)strlen(label) * cell;
    const int unitW = (int)strlen(unit)  * cell;
    const int numW  = measureBitmapText(DSEG7_72, value);
    const int total = lblW + GAP + numW + GAP + unitW;
    const int x0    = RIGHT_X + (USR_W - RIGHT_X - total) / 2;
    const int textY = y + (72 - textH) / 2;  // centre labels against the hero

    drawTextU(x0, textY, 1, 2, COL_AMBER, label);
    drawBitmapText(x0 + lblW + GAP, y, DSEG7_72, value, col);
    drawTextU(x0 + lblW + GAP + numW + GAP, textY, 1, 2, col, unit);
}

void displayGps() {
    const bool     hasFix = gpsHasFix();
    const GpsLink  link   = gpsLink();
    const bool     ready  = (link == GpsLink::READY);
    const uint8_t  sats   = gpsSatellites();
    const float    hdop   = gpsHdop();
    const float    grade  = gpsGradePct();

    fillScreenU(COL_BG);

    // --- LEFT: altitude hero ---------------------------------------------
    drawCenteredInU(0, LEFT_W, TOP_LABEL_Y, 1, 2, COL_AMBER, "ALTITUDE");

    char heroBuf[8];
    if (hasFix) snprintf(heroBuf, sizeof(heroBuf), "%d", (int)(gpsAltitudeM() + 0.5f));
    else        snprintf(heroBuf, sizeof(heroBuf), "--");

    const uint16_t heroCol  = hasFix ? COL_AMBER : COL_HOT;
    const int      unitW    = fontCellLong(2, 2);         // "M" at 16x32 z2 → 32
    constexpr int  HERO_GAP = 12;

    // Altitude is 2 digits near the coast but 4 up on a serra, and DSEG7_120
    // digits advance 98 px — four of them plus the unit overflow the 340 px
    // column. Step down to the 72 px face when the wide one will not fit.
    const int wide120 = measureBitmapText(DSEG7_120, heroBuf) + HERO_GAP + unitW;
    const bool useBig = wide120 <= (LEFT_W - 2 * HERO_MARGIN);

    const BitmapFont& heroFont = useBig ? DSEG7_120 : DSEG7_72;
    const int heroH  = useBig ? 120 : 72;
    const int heroY  = HERO_Y + (120 - heroH) / 2;
    const int heroW  = measureBitmapText(heroFont, heroBuf);
    const int heroX  = (LEFT_W - (heroW + HERO_GAP + unitW)) / 2;

    drawBitmapText(heroX, heroY, heroFont, heroBuf, heroCol);
    drawTextU(heroX + heroW + HERO_GAP, heroY + (heroH - 64) / 2, 2, 2,
              heroCol, "M");

    // --- DIVIDERS ---------------------------------------------------------
    fillRectU(DIV_X, DIV_Y_TOP, DIV_W, FOOTER_DIV - DIV_Y_TOP, COL_AMBER);
    fillRectU(0,     FOOTER_DIV, USR_W, 2,                     COL_AMBER);

    // --- RIGHT: grade / speed / heading ----------------------------------
    char valBuf[12];

    // Grade is NAN whenever the gates in GpsModule refuse to vouch for it —
    // render "--" instead of a number the sensor cannot support.
    if (isnan(grade)) snprintf(valBuf, sizeof(valBuf), "--");
    else              snprintf(valBuf, sizeof(valBuf), "%.1f", grade);
    drawRow(ROW_GRADE_Y, "INCL", valBuf, "%", gradeColor(grade));

    if (hasFix) snprintf(valBuf, sizeof(valBuf), "%d", (int)(gpsSpeedKmh() + 0.5f));
    else        snprintf(valBuf, sizeof(valBuf), "--");
    drawRow(ROW_SPEED_Y, "VEL", valBuf, "KM/H", hasFix ? COL_AMBER : COL_HOT);

    // Heading: cardinal point big, degrees small beside it. All letters, so
    // CGROM rather than the seven-segment font.
    {
        const char* card = gpsCardinal();
        char degBuf[10];
        if (hasFix) snprintf(degBuf, sizeof(degBuf), "%03d", (int)(gpsCourseDeg() + 0.5f));
        else        snprintf(degBuf, sizeof(degBuf), "---");

        constexpr int GAP = 24;
        const int cardH   = fontCellShort(2, 2);   // 64 px tall — see ROW_HDG_Y
        const int degH    = fontCellShort(1, 2);   // 48 px tall
        const int cardW   = (int)strlen(card)   * fontCellLong(2, 2);
        const int degW    = (int)strlen(degBuf) * fontCellLong(1, 2);
        const int total   = cardW + GAP + degW;
        const int x0      = RIGHT_X + (USR_W - RIGHT_X - total) / 2;
        const uint16_t c  = hasFix ? COL_AMBER : COL_HOT;

        drawTextU(x0, ROW_HDG_Y, 2, 2, c, card);
        drawTextU(x0 + cardW + GAP, ROW_HDG_Y + (cardH - degH) / 2, 1, 2, c, degBuf);
    }

    // --- FOOTER: link/fix state, satellites, HDOP ------------------------
    char buf[24];

    // While the UBX handshake runs, the first third reports it instead of the
    // fix — that makes a module swap being reconfigured visible on the dash,
    // with no serial monitor attached.
    if (!ready) {
        snprintf(buf, sizeof(buf), "GPS %s", gpsLinkLabel());
        drawCenteredInU(0, 320, FOOTER_Y, 1, 2,
                        (link == GpsLink::ABSENT) ? COL_HOT : COL_AMBER, buf);
    } else {
        snprintf(buf, sizeof(buf), "%s", hasFix ? "FIX OK" : "SEM FIX");
        drawCenteredInU(0, 320, FOOTER_Y, 1, 2,
                        hasFix ? COL_GOOD : COL_HOT, buf);
    }

    snprintf(buf, sizeof(buf), "SAT %02d", (int)sats);
    drawCenteredInU(320, 640, FOOTER_Y, 1, 2, fixColor(hasFix, sats, hdop), buf);

    if (hasFix) snprintf(buf, sizeof(buf), "HDOP %.1f", hdop);
    else        snprintf(buf, sizeof(buf), "HDOP --");
    drawCenteredInU(640, USR_W, FOOTER_Y, 1, 2,
                    fixColor(hasFix, sats, hdop), buf);
}

ResetSet gpsResets() {
    return { 0, { { nullptr, nullptr }, { nullptr, nullptr } } };
}
