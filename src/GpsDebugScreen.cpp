#include "GpsDebugScreen.h"
#include "GpsModule.h"
#include "Layout.h"
#include "Tft.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

using namespace pobc;

// Dense, text-only layout: this is an instrument for reading numbers, not a
// driving display, so CGROM 12x24 at zoom 1 is used throughout (12 px per
// char, 80 columns across the 960 px panel). No DSEG7 — everything here has
// letters in it.

static constexpr int FONT = 1;   // 12x24
static constexpr int ZOOM = 1;

static constexpr int Y_TITLE  =   6;
static constexpr int Y_DIV_1  =  34;
static constexpr int Y_CFG_1  =  42;
static constexpr int Y_CFG_2  =  70;
static constexpr int Y_CFG_3  =  98;
static constexpr int Y_DIV_2  = 128;
static constexpr int Y_STAT_1 = 136;
static constexpr int Y_STAT_2 = 164;
static constexpr int Y_GRADE  = 192;
static constexpr int Y_POS    = 220;

// Three columns for the 3x3 configuration grid.
static constexpr int COL_X[3] = { 12, 332, 652 };

static const char* gateLabel(GpsGradeGate g) {
    switch (g) {
        case GpsGradeGate::OK:       return "OK";
        case GpsGradeGate::NO_FIX:   return "SEM FIX";
        case GpsGradeGate::HDOP:     return "HDOP ALTO";
        case GpsGradeGate::SPEED:    return "LENTO";
        case GpsGradeGate::BASELINE: return "ACUMULANDO";
    }
    return "?";
}

void displayGpsDebug() {
    fillScreenU(COL_BG);

    char buf[48];

    // --- Title: link state, baud, how many times config was applied -------
    const GpsLink link  = gpsLink();
    const bool    ready = (link == GpsLink::READY);
    const bool    cfgOk = gpsCfgAllOk();

    drawTextU(12, Y_TITLE, FONT, ZOOM, COL_AMBER, "GPS DEBUG");

    snprintf(buf, sizeof(buf), "LINK %s", gpsLinkLabel());
    drawTextU(COL_X[1], Y_TITLE, FONT, ZOOM,
              ready ? COL_GOOD : COL_HOT, buf);

    snprintf(buf, sizeof(buf), "%lu bps", (unsigned long)gpsBaud());
    drawTextU(COL_X[1] + 200, Y_TITLE, FONT, ZOOM, COL_AMBER, buf);

    // gpsConfigRuns() is the headline number: 1 on a fresh module and then
    // stable forever. Climbing on every boot means the save did not stick.
    const uint32_t runs = gpsConfigRuns();
    snprintf(buf, sizeof(buf), "CFG %s  RUNS %lu",
             cfgOk ? "OK" : "DIVERGE", (unsigned long)runs);
    drawTextU(COL_X[2], Y_TITLE, FONT, ZOOM,
              cfgOk ? COL_GOOD : COL_HOT, buf);

    fillRectU(0, Y_DIV_1, USR_W, 2, COL_AMBER);

    // --- Configuration grid: want vs got, straight from the receiver ------
    // 9 items (DYN / RATE / BAUD / the six NMEA sentences) laid out 3x3.
    const int rowY[3] = { Y_CFG_1, Y_CFG_2, Y_CFG_3 };
    const uint8_t n = gpsCfgCount();
    for (uint8_t i = 0; i < n; ++i) {
        const GpsCfgItem& it = gpsCfgItem(i);
        const int col = i / 3;
        const int row = i % 3;
        if (col > 2 || row > 2) continue;

        if (it.got == GPS_CFG_NO_REPLY) {
            snprintf(buf, sizeof(buf), "%-5s %ld / --", it.name, (long)it.want);
        } else {
            snprintf(buf, sizeof(buf), "%-5s %ld / %ld",
                     it.name, (long)it.want, (long)it.got);
        }
        drawTextU(COL_X[col], rowY[row], FONT, ZOOM,
                  it.ok ? COL_GOOD : COL_HOT, buf);
    }

    fillRectU(0, Y_DIV_2, USR_W, 2, COL_AMBER);

    // --- Link health -----------------------------------------------------
    const uint32_t ck = gpsFailedChecksum();
    const uint32_t nak = gpsNakCount();

    snprintf(buf, sizeof(buf), "CHARS %lu", (unsigned long)gpsCharsProcessed());
    drawTextU(COL_X[0], Y_STAT_1, FONT, ZOOM, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "CKERR %lu", (unsigned long)ck);
    drawTextU(COL_X[1], Y_STAT_1, FONT, ZOOM, ck ? COL_HOT : COL_GOOD, buf);

    snprintf(buf, sizeof(buf), "ACK %lu  NAK %lu",
             (unsigned long)gpsAckCount(), (unsigned long)nak);
    drawTextU(COL_X[2], Y_STAT_1, FONT, ZOOM, nak ? COL_HOT : COL_AMBER, buf);

    // --- Fix quality -----------------------------------------------------
    const bool  hasFix = gpsHasFix();
    const float hdop   = gpsHdop();

    snprintf(buf, sizeof(buf), "SAT %02d", (int)gpsSatellites());
    drawTextU(COL_X[0], Y_STAT_2, FONT, ZOOM,
              hasFix ? COL_GOOD : COL_HOT, buf);

    snprintf(buf, sizeof(buf), "HDOP %.1f", hdop);
    drawTextU(COL_X[0] + 140, Y_STAT_2, FONT, ZOOM,
              (hasFix && hdop <= 2.0f) ? COL_GOOD : COL_AMBER, buf);

    if (hasFix) snprintf(buf, sizeof(buf), "IDADE %lums",
                         (unsigned long)gpsFixAgeMs());
    else        snprintf(buf, sizeof(buf), "IDADE --");
    drawTextU(COL_X[1], Y_STAT_2, FONT, ZOOM,
              hasFix ? COL_AMBER : COL_HOT, buf);

    // Raw vs smoothed altitude side by side: the gap is the EMA at work, and
    // a wild raw value explains a jumpy grade.
    snprintf(buf, sizeof(buf), "ALT %.1f  EMA %.1f",
             gpsAltitudeRawM(), gpsAltitudeM());
    drawTextU(COL_X[2], Y_STAT_2, FONT, ZOOM, COL_AMBER, buf);

    // --- Grade derivation ------------------------------------------------
    const GpsGradeGate gate = gpsGradeGate();
    const float grade = gpsGradePct();

    snprintf(buf, sizeof(buf), "GATE %s", gateLabel(gate));
    drawTextU(COL_X[0], Y_GRADE, FONT, ZOOM,
              (gate == GpsGradeGate::OK) ? COL_GOOD : COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "BASE %d/%d m",
             (int)gpsGradeAnchorM(), (int)gpsGradeBaselineM());
    drawTextU(COL_X[1], Y_GRADE, FONT, ZOOM, COL_AMBER, buf);

    if (isnan(grade)) snprintf(buf, sizeof(buf), "INCL --");
    else              snprintf(buf, sizeof(buf), "INCL %.2f %%", grade);
    drawTextU(COL_X[2], Y_GRADE, FONT, ZOOM, COL_AMBER, buf);

    // --- Position --------------------------------------------------------
    // Deliberately absent from the driving screen; useful here to cross-check
    // against a phone.
    if (hasFix) {
        snprintf(buf, sizeof(buf), "LAT %.5f   LON %.5f", gpsLat(), gpsLon());
    } else {
        snprintf(buf, sizeof(buf), "LAT --   LON --");
    }
    drawTextU(COL_X[0], Y_POS, FONT, ZOOM,
              hasFix ? COL_AMBER : COL_HOT, buf);

    snprintf(buf, sizeof(buf), "VEL %.1f km/h", gpsSpeedKmh());
    drawTextU(COL_X[2], Y_POS, FONT, ZOOM, COL_AMBER, buf);

    // --- Footer ----------------------------------------------------------
    fillRectU(0, FOOTER_DIV, USR_W, 2, COL_AMBER);
    drawCenteredInU(0, USR_W, FOOTER_Y, FONT, ZOOM, COL_AMBER,
                    "DEBUG  -  R+S juntos 1.2s para ocultar");
}

ResetSet gpsDebugResets() {
    return { 0, { { nullptr, nullptr }, { nullptr, nullptr } } };
}
